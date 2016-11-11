#include <iostream>     // for cout
#include "Graph2D.h"
#include "cs1037lib-time.h" // for basic timing/pausing
using namespace std;

#define TERMINAL ( (arc *) 1 )		// COPY OF DEFINITION FROM maxflow.cpp
#define INFTY 100
//arbitrary
#define SIGMA 10
#define TRUNC_THRESH 100


// function for printing error messages from the max-flow library
inline void mf_assert(char * msg) { cout << "Error from max-flow library: " << msg << endl; cin.ignore(); exit(1); }

// function for computing edge weights from intensity differences
int fn(const double dI, double sigma) { return (int)(5.0 / (1.0 + (dI*dI / (sigma*sigma)))); }  // function used for setting "n-link costs" ("Sensitivity" sigme is a tuning parameter)

Graph2D::Graph2D()
	: Graph<int, int, int>(0, 0, mf_assert)
	, m_nodeFlow()
	, m_seeds()
	, m_box(0)
	, m_labeling()
{}

void Graph2D::reset(Table2D<RGB> & im, double sigma, double r, bool eraze_seeds)
{
	cout << "resetting segmentation" << endl;
	int x, y, wR, wD, height = im.getHeight(), width = im.getWidth();
	Point p;
	m_nodeFlow.reset(width, height, 0); // NOTE: width and height of m_nodeFlow table 
										// are used in functions to_node() and to_Point(), 
										// Thus, m_nodeFlow MUST BE RESET FIRST!!!!!!!!
	Graph<int, int, int>::reset();
	add_node(width*height);    // adding nodes
	for (y = 0; y<(height - 1); y++) for (x = 0; x<(width - 1); x++) { // adding edges (n-links)
		node_id n = to_node(x, y);
		wR = fn(dI(im[x][y], im[x + 1][y]), sigma);   // function dI(...) is declared in Image2D.h
		wD = fn(dI(im[x][y], im[x][y + 1]), sigma); // function fn(...) is declared at the top of this file
		add_edge(n, n + 1, wR, wR);
		add_edge(n, n + width, wD, wD);
	}

	m_labeling.reset(width, height, NONE);
	if (eraze_seeds) { m_seeds.reset(width, height, NONE); m_box.clear(); } // remove hard constraints and/or box
	else { // resets hard "constraints" (seeds)
		cout << "keeping seeds" << endl;
		for (p.y = 0; p.y<height; p.y++) for (p.x = 0; p.x<width; p.x++) {
			if (m_seeds[p] == OBJ) add_tweights(p, INFTY, 0);
			else if (m_seeds[p] == BKG) add_tweights(p, 0, INFTY);
		}
	}
	m_r = r;
}


// GUI calls this function when a user left- or right-clicks on an image pixel while in "BRUSH" mode
void Graph2D::addSeed(Point& p, Label seed_type)
{
	if (!m_seeds.pointIn(p)) return;
	Label current_constraint = m_seeds[p];
	if (current_constraint == seed_type) return;

	if (current_constraint == NONE) {
		if (seed_type == OBJ) add_tweights(p, INFTY, 0);
		else                add_tweights(p, 0, INFTY);
	}
	else if (current_constraint == OBJ) {
		if (seed_type == BKG) add_tweights(p, -INFTY, INFTY);
		else                add_tweights(p, -INFTY, 0);
	}
	else if (current_constraint == BKG) {
		if (seed_type == OBJ) add_tweights(p, INFTY, -INFTY);
		else                add_tweights(p, 0, -INFTY);
	}
	m_seeds[p] = seed_type;
}

// GUI calls this function when a user left- or right-clicks and drags on image pixels while in "BOX" mode
// the first click defines the top-left corner (tl) of the box, while the unclick point defines the bottom-right (br) corner.
void Graph2D::setBox(Point& tl, Point& br)
{
	while (m_box.size()<2) m_box.push_back(Point(0, 0));
	m_box[0] = tl;
	m_box[1] = br;
}


inline Label Graph2D::what_label(Point& p)
{
	node_id i = to_node(p);
	if (nodes[i].parent) return (nodes[i].is_sink) ? BKG : OBJ;
	else                 return NONE;
}

void Graph2D::setLabeling()
{
	int width = m_labeling.getWidth();
	int height = m_labeling.getHeight();
	Point p;
	for (p.y = 0; p.y<height; p.y++) for (p.x = 0; p.x<width; p.x++) m_labeling[p] = what_label(p);

	cout << "Finished setting labelling" << endl;
}

//mode - WINDOW, SCANLINE, GLOBAL
//image - left image
//image_R - right image
//w - window size
int Graph2D::stereo(Mode mode, Table2D<RGB> image, Table2D<RGB> image_R, int w, int l, SCANLINE_MODE scanline_mode, void(*draw_function)) {
	int width = image.getWidth(),
		height = image.getHeight();
	if (mode == WINDOW) {
		//disparities holds differences in x between a pixel and its partner on the other image
		vector<vector<int>> disparities;
		disparities.resize(width, vector<int>(height, 0));

		//window goes w pixels to the left and w pixels to the right, so loop from w -> width - w
		for (int x = w; x < width - w; x++) {
			cout << x << "/" << width << endl;
			for (int y = w; y < height - w; y++) {
				int min_energy_index = -1;
				int min_energy = INT_MAX;

				//loop through the current row, from first window (starts at pixel w, y) to last (width - w, y) and calculate error energy
				//this loop variable i is used to index the right image
				for (int i = w; i < x; i++) {
					int cum_energy = 0;
					//loop through window - w is window size
					for (int winx = -w; winx <= w; winx++) {
						for (int winy = -w; winy <= w; winy++) {
							Point lp = { x + winx , y + winy };
							Point rp = { i + winx, y + winy };
							int r = image[lp].r - image_R[rp].r,
								g = image[lp].g - image_R[rp].g,
								b = image[lp].b - image_R[rp].b;
							//int diff = dI(image[lp], image_R[rp]);

							//sum of abs differences
							cum_energy += abs(r) + abs(g) + abs(b);
							//cum_energy += diff;
						}
					}
					//see if we found a new minimum energy (matching pixel) at current i
					if (cum_energy < min_energy) {
						min_energy = cum_energy;
						min_energy_index = i;
					}
				}
				disparities[x][y] = x - min_energy_index;
			}
		}
		//now, disparities is populated
		//need to create some way to compare the disparities to one another - and separate them into depths
		int largest_disparity = -1;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				if (disparities[x][y] > largest_disparity)
					largest_disparity = disparities[x][y];
			}
		}
		//create disparity level 'bins'

		//for some reason, dividing by 8 segments the image very nicely
		int bin_width = width / l / 8;

		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				Point p = { x,y };
				int bin = (int)(round(disparities[x][y] / bin_width));
 				if (bin >= l)
					bin = l;
				//cout << x << "," << y << " Label: " << bin << endl;
				m_labeling[p] = bin;
			}
		}
	}
	
	//does not set labeling right now because everything is placed in the source for some reason
	//probably not using graph library correctly..
	else if (mode == GLOBAL) {
		//create 3d graph for image, with l depth per pixel, 3 edges per node (right/down/upper)
		//Graph graph = Graph<int, int, int>(l*width*height, l*width*height*3);
		//initialize
		add_node(l*width*height);

		//loop through entire graph
		//loop to width/height -1 since will add 1 later in each direction 
		for (int x = l; x < width - 1; x++) {
			for (int y = l; y < height - 1; y++) {
				for (int i = 0; i < l - 1; i++) {
					//current node
					node_id node = to_node(x, y) + i*width*height;
					//nlinks (inspired by reset())

					//get right, lower intensity differences
					int right_idiff = fn(dI(image[x][y], image_R[x + 1][y]), SIGMA);
					int lower_idiff = fn(dI(image[x][y], image_R[x][y + 1]), SIGMA);
					//add diffs to graph
					add_edge(node, node + 1, right_idiff, right_idiff);
					add_edge(node, node + width, lower_idiff, lower_idiff);
					
					//compute and set all disparity links
					if (i >= x) {
						add_tweights(to_point(node), INT_MAX, INT_MAX);
					}
					else {
						int source_r = image[x][y].r - image_R[x - i - 1][y].r,
							source_g = image[x][y].g - image_R[x - i - 1][y].g,
							source_b = image[x][y].b - image_R[x - i - 1][y].b,
							sink_r = image[x][y].r - image_R[x - i][y].r,
							sink_g = image[x][y].g - image_R[x - i][y].g,
							sink_b = image[x][y].b - image_R[x - i][y].b;
							
						//cumulative source/sink differences
						int cum_source	= abs(source_r) + abs(source_g) + abs(source_b);
						int cum_sink	= abs(sink_r)   + abs(sink_g)   + abs(sink_b);

						//add the computed tweights
						//graph.add_tweights(node, cum_source, cum_sink);
						add_tweights(to_point(node), cum_source, cum_sink);
					}					
				}
			}
		}
		//don't care about draw_function just use null
		int flow = maxflow(NULL);

		//set labels
		for (int x = l; x < width - 1; x++) {
			for (int y = l; y < height - 1; y++) {
				for (int i = 0; i < l - 1; i++) {
					//current node, same as above
					node_id node = to_node(x, y) + i*width*height;

					//sink = 1, source = 0

					//find out where the cut is between source and sink. label x,y that i value
					//for some reason they are all 0s so this block is never executed, which is why nothing shows up
					//cout << graph.what_segment(node) << endl;
					//if (graph.what_segment(node)) {
					if (what_segment(node)) {
						//belongs to sink, ith layer
						Point p = { x,y };
						m_labeling[p] = i;
					}
				}
			}
		}
		setLabeling();
	}

	else if (mode == SCANLINE) {
		//3 edges per node (2 Coccl and corresp.) graph is width x width
		//Graph graph = Graph<int, int, int>(width*width, width*width*3);
		add_node(width*width);

		for (int y = 0; y < height; y++) {
			//image_L index
			for (int x = 0; x < width - 1; x++) {
				//image_R index
				for (int i = 0; i < width - 1; i++) {
					//Cocclusions					
					int right_idiff = fn(dI(image_R[x][y],	image_R[x + 1][y]), SIGMA);
					int lower_idiff = fn(dI(image[i][y],	image[i + 1][y]), SIGMA);

					int corresp_r =	abs(image[x][y].r - image_R[i][y].r),
						corresp_g =	abs(image[x][y].g - image_R[i][y].g),
						corresp_b =	abs(image[x][y].b - image_R[i][y].b);

					//if ABSOLUTE, do nothing
					if (scanline_mode == QUADRATIC) {
						corresp_r *= corresp_r;
						corresp_g *= corresp_g;
						corresp_b *= corresp_b;
					}
					else if (scanline_mode == TRUNCATED) {
						corresp_r = corresp_r > TRUNC_THRESH ? TRUNC_THRESH : corresp_r;
						corresp_g = corresp_g > TRUNC_THRESH ? TRUNC_THRESH : corresp_g;
						corresp_b = corresp_b > TRUNC_THRESH ? TRUNC_THRESH : corresp_b;
					}

					int corresp = corresp_r + corresp_g + corresp_b;

					//node's coordinates are x,i (right image index is y-axis)
					node_id node = to_node(x, i);
					//right edge
					add_edge(node, node + 1, right_idiff, right_idiff);
					//down edge
					add_edge(node, node + width, lower_idiff, lower_idiff);
					//diagonal edge
					add_edge(node, node + 1 + width, corresp, corresp);
				}
			}
		}
		//graph is built
		//now, find shortest path		
		//but how to find the shortest path using this library?
		//then, set labelling:
		//the further the current path node is from the diagonal, the greater the disparity
		//the 'distance' from the 'perfect' node (on the diagonal) to the current node gives the disparity
		//then labelling can be set the same way it is in WINDOW mode, using bins calculated from the max disparity
		
		//commented until I figure out how to implement get_shortest_path. until then this is more like pseudocode
		/*
		vector<node> diagonal;
		for(int x = 0; x < width; x++) 
			for(int y = 0; y < width; y++)
				diagonal.push_back(to_node(x,y));
		// assume a method called get_shortest path. this is what is missing...
		vector<node> shortest_path = get_shortest_path();
		
		int max_dist = 0, dist = 0;
		int row = 0, col = 0;
		int i = 0;
		vector<vector<int>> dists;
		node previous;
		while(i < shortest_path.size()) {
			//diagonal edge
			if(shortest_path[i+1] == previous+1+width) {
				i++;
				row++;
				col++;
				dist = 0;
				continue;
			}
			//horizontal
			if(shortest_path[i+1] == previous+1) {
				dist++;
				col++;
			}
			//vertical
			if(shortest_path[i+1] == previous+width) {
				dist--;
				row++;
			}

			dists[row][col] = dist;

			if(dist > max_dist)
				max_dist = dist;
		}
		//label the pixels
		int bin_size = max_dist/l;
		for(int x = 0; x < width; x++) {
			for(int y = 0; y < height; y++) {
				m_labeling[x][y] = round(max_dist / dists[x][y]);
			}
		}
		setLabeling();
		*/
	}

	//setLabeling(); 
	cout << "Finished stereo imaging" << endl;
	return 0;
}

int Graph2D::compute_mincut(Mode mode, Table2D<RGB> im, void(*draw_function)())
{
	int flow = maxflow(draw_function);
	setLabeling();
	return flow;
}


// overwrites the base class function to allow visualization
// (e.g. calls specified "draw_function" to show each flow augmentation)
int Graph2D::maxflow(void(*draw_function)())
{
	node *i, *j, *current_node = NULL;
	arc *a;
	nodeptr *np, *np_next;

	if (!nodeptr_block)
	{
		nodeptr_block = new DBlock<nodeptr>(NODEPTR_BLOCK_SIZE, error_function);
	}

	changed_list = NULL;
	maxflow_init();

	// main loop
	while (1)
	{
		// test_consistency(current_node);

		if ((i = current_node))
		{
			i->next = NULL; /* remove active flag */
			if (!i->parent) i = NULL;
		}
		if (!i)
		{
			if (!(i = next_active())) break;
		}

		/* growth */
		if (!i->is_sink)
		{
			/* grow source tree */
			for (a = i->first; a; a = a->next)
				if (a->r_cap)
				{
					j = a->head;
					if (!j->parent)
					{
						j->is_sink = 0;
						j->parent = a->sister;
						j->TS = i->TS;
						j->DIST = i->DIST + 1;
						set_active(j);
						add_to_changed_list(j);
					}
					else if (j->is_sink) break;
					else if (j->TS <= i->TS &&
						j->DIST > i->DIST)
					{
						/* heuristic - trying to make the distance from j to the source shorter */
						j->parent = a->sister;
						j->TS = i->TS;
						j->DIST = i->DIST + 1;
					}
				}
		}
		else
		{
			/* grow sink tree */
			for (a = i->first; a; a = a->next)
				if (a->sister->r_cap)
				{
					j = a->head;
					if (!j->parent)
					{
						j->is_sink = 1;
						j->parent = a->sister;
						j->TS = i->TS;
						j->DIST = i->DIST + 1;
						set_active(j);
						add_to_changed_list(j);
					}
					else if (!j->is_sink) { a = a->sister; break; }
					else if (j->TS <= i->TS &&
						j->DIST > i->DIST)
					{
						/* heuristic - trying to make the distance from j to the sink shorter */
						j->parent = a->sister;
						j->TS = i->TS;
						j->DIST = i->DIST + 1;
					}
				}
		}

		TIME++;

		if (a)
		{
			i->next = i; /* set active flag */
			current_node = i;

			// the line below can be uncommented to visualize current SOURCE and SINK trees
			//... if (draw_function) {setLabeling(); draw_function(); Pause(1);}

			/* augmentation */
			augment(a); // uses overwritten function of the base class (see code below)
						// that keeps track of flow in table m_nodeFlow
			if (draw_function) { draw_function(); Pause(1); }
			/* augmentation end */

			/* adoption */
			while ((np = orphan_first))
			{
				np_next = np->next;
				np->next = NULL;

				while ((np = orphan_first))
				{
					orphan_first = np->next;
					i = np->ptr;
					nodeptr_block->Delete(np);
					if (!orphan_first) orphan_last = NULL;
					if (i->is_sink) process_sink_orphan(i);
					else            process_source_orphan(i);
				}

				orphan_first = np_next;
			}
			/* adoption end */
		}
		else current_node = NULL;
	}
	// test_consistency();

	delete nodeptr_block;
	nodeptr_block = NULL;

	maxflow_iteration++;
	return flow;
}

// overwrites the base class function (to save the nodeFlow in a 2D array m_nodeFlow)
void Graph2D::augment(arc *middle_arc)
{
	node *i;
	arc *a;
	int bottleneck;


	/* 1. Finding bottleneck capacity */
	/* 1a - the source tree */
	bottleneck = middle_arc->r_cap;
	for (i = middle_arc->sister->head; ; i = a->head)
	{
		a = i->parent;
		if (a == TERMINAL) break;
		if (bottleneck > a->sister->r_cap) bottleneck = a->sister->r_cap;
	}
	if (bottleneck > i->tr_cap) bottleneck = i->tr_cap;
	/* 1b - the sink tree */
	for (i = middle_arc->head; ; i = a->head)
	{
		a = i->parent;
		if (a == TERMINAL) break;
		if (bottleneck > a->r_cap) bottleneck = a->r_cap;
	}
	if (bottleneck > -i->tr_cap) bottleneck = -i->tr_cap;


	/* 2. Augmenting */
	/* 2a - the source tree */
	middle_arc->sister->r_cap += bottleneck;
	middle_arc->r_cap -= bottleneck;
	for (i = middle_arc->sister->head; ; i = a->head)
	{
		m_nodeFlow[to_point((node_id)(i - nodes))] = 1;
		a = i->parent;
		if (a == TERMINAL) break;
		a->r_cap += bottleneck;
		a->sister->r_cap -= bottleneck;
		if (!a->sister->r_cap)
		{
			set_orphan_front(i); // add i to the beginning of the adoption list
		}
	}
	i->tr_cap -= bottleneck;
	if (!i->tr_cap)
	{
		set_orphan_front(i); // add i to the beginning of the adoption list
	}
	/* 2b - the sink tree */
	for (i = middle_arc->head; ; i = a->head)
	{
		m_nodeFlow[to_point((node_id)(i - nodes))] = 1;
		a = i->parent;
		if (a == TERMINAL) break;
		a->sister->r_cap += bottleneck;
		a->r_cap -= bottleneck;
		if (!a->r_cap)
		{
			set_orphan_front(i); // add i to the beginning of the adoption list
		}
	}
	i->tr_cap += bottleneck;
	if (!i->tr_cap)
	{
		set_orphan_front(i); // add i to the beginning of the adoption list
	}


	flow += bottleneck;
}
