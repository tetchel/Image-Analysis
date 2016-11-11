#include <iostream>     // for cout
#include "Graph2D.h"
#include "cs1037lib-time.h" // for basic timing/pausing
using namespace std;

#define TERMINAL ( (arc *) 1 )		// COPY OF DEFINITION FROM maxflow.cpp


// function for printing error messages from the max-flow library
inline void mf_assert(char * msg) {cout << "Error from max-flow library: " << msg << endl; cin.ignore(); exit(1);}

// function for computing edge weights from intensity differences
int fn(const double dI, double sigma) {return (int) (5.0/(1.0+(dI*dI/(sigma*sigma))));}  // function used for setting "n-link costs" ("Sensitivity" sigme is a tuning parameter)

Graph2D::Graph2D()
: Graph<int,int,int>(0,0,mf_assert)
, m_nodeFlow() 
, m_seeds()
, m_box(0)
, m_labeling()
{}

void Graph2D::reset(Table2D<RGB> & im, double sigma, double r, bool eraze_seeds) 
{
	cout << "resetting segmentation" << endl;
	int x, y, wR, wD, height = im.getHeight(), width =  im.getWidth();
	Point p;
	m_nodeFlow.reset(width,height,0); // NOTE: width and height of m_nodeFlow table 
	                                  // are used in functions to_node() and to_Point(), 
	                                  // Thus, m_nodeFlow MUST BE RESET FIRST!!!!!!!!
	Graph<int,int,int>::reset();
	add_node(width*height);    // adding nodes
	for (y=0; y<(height-1); y++) for (x=0; x<(width-1); x++) { // adding edges (n-links)
		node_id n = to_node(x,y);
		wR=fn(dI(im[x][y],im[x+1][y]),sigma);   // function dI(...) is declared in Image2D.h
		wD=fn(dI(im[x][y],im[x  ][y+1]),sigma); // function fn(...) is declared at the top of this file
		add_edge( n, n+1,     wR, wR );
		add_edge( n, n+width, wD, wD );	
	}

	m_labeling.reset(width,height,NONE);
	if (eraze_seeds) { m_seeds.reset(width,height,NONE); m_box.clear();} // remove hard constraints and/or box
	else { // resets hard "constraints" (seeds)
		cout << "keeping seeds" << endl;
		for (p.y=0; p.y<height; p.y++) for (p.x=0; p.x<width; p.x++) { 
			if      (m_seeds[p]==OBJ) add_tweights( p, INFTY, 0   );
			else if (m_seeds[p]==BKG) add_tweights( p,   0, INFTY );
		}
	}
	m_r = r;
}


// GUI calls this function when a user left- or right-clicks on an image pixel while in "BRUSH" mode
void Graph2D::addSeed(Point& p, Label seed_type) 
{
	if (!m_seeds.pointIn(p)) return;
	Label current_constraint = m_seeds[p];
	if (current_constraint==seed_type) return;

	if (current_constraint==NONE) {
		if (seed_type==OBJ) add_tweights( p, INFTY, 0   );
		else                add_tweights( p,   0, INFTY );
	}
	else if (current_constraint==OBJ) {
		if (seed_type==BKG) add_tweights( p, -INFTY, INFTY );
		else                add_tweights( p, -INFTY,   0   );
	}
	else if (current_constraint==BKG) {
		if (seed_type==OBJ) add_tweights( p, INFTY, -INFTY );
		else                add_tweights( p,   0,   -INFTY );
	}
	m_seeds[p]=seed_type;
}

// GUI calls this function when a user left- or right-clicks and drags on image pixels while in "BOX" mode
// the first click defines the top-left corner (tl) of the box, while the unclick point defines the bottom-right (br) corner.
void Graph2D::setBox(Point& tl, Point& br) 
{
	while (m_box.size()<2) m_box.push_back(Point(0,0));
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
	int width  = m_labeling.getWidth();
	int height = m_labeling.getHeight();
	Point p;
	for (p.y=0; p.y<height; p.y++) for (p.x=0; p.x<width; p.x++) m_labeling[p]=what_label(p);
}

#define NUM_BINS 8
#define BIN_SIZE 32.0
#define BCD_THRESHHOLD 0.0001
const int INFTY = 100;				// value of t-links corresponding to hard-constraints 

int Graph2D::compute_mincut(Mode mode, Table2D<RGB> im, void (*draw_function)())
{		
	//set initial t-weights for both color modes
	//further processing will be done for color-e, but this is all for f
	if (mode != EDGE) {
		//histograms as 3d arrays. each index will hold the number of pixels that fall into that bin.
		double	obj_histo[NUM_BINS][NUM_BINS][NUM_BINS] = { 0 },
				bkg_histo[NUM_BINS][NUM_BINS][NUM_BINS] = { 0 };

		//number of seeds given for obj, bkg
		//used for calculating probabilities
		unsigned obj_seeds = 0,
				 bkg_seeds = 0;

		//build histogram from seed intensities
		for (unsigned x = 0; x < im.getWidth(); x++) {
			for (unsigned y = 0; y < im.getHeight(); y++) {
				Point p = { (int)x, (int)y };
				//don't care about non-seeds
				if (m_seeds[p] != NONE) {
					//calculate r, g, b bins
					int r = im[x][y].r / BIN_SIZE,
						g = im[x][y].g / BIN_SIZE,
						b = im[x][y].b / BIN_SIZE;

					//see if this pixel belongs in obj or bkg histogram (based on seed type)
					if (m_seeds[p] == OBJ) {
						obj_seeds++;
						obj_histo[r][g][b]++;
					}
					else if (m_seeds[p] == BKG) {
						bkg_seeds++;
						bkg_histo[r][g][b]++;
					}
				}
			}
		}
		
		//for each item in the histograms, use the intensities to calculate:
		//-lnPr(I | theta)
		for (unsigned i = 0; i < NUM_BINS; i++) {
			for (unsigned j = 0; j < NUM_BINS; j++) {
				for (unsigned k = 0; k < NUM_BINS; k++) {
					//log = ln
					//number of items in that bin / number of seeds
					//gives probability 
					double result = -log(obj_histo[i][j][k] / obj_seeds);
					//correct for max value if necessary
					if (result > INFTY)
						result = INFTY;
					obj_histo[i][j][k] = m_r * result;

					//same process for background seeds
					result = -log(bkg_histo[i][j][k] / bkg_seeds);
					if (result > INFTY)
						result = INFTY;
					bkg_histo[i][j][k] = m_r * result;
				}
			}
		}

		//calculate t-link weights for unseeded pixels
		for (unsigned x = 0; x < im.getWidth(); x++) {
			for (unsigned y = 0; y < im.getHeight(); y++) {
				Point p = { (int)x, (int)y };
				//ignore seeds
				if (m_seeds[p] == NONE) {
					int r = im[x][y].r / BIN_SIZE,
						g = im[x][y].g / BIN_SIZE,
						b = im[x][y].b / BIN_SIZE;

					//add this tweight
					add_tweights(p, bkg_histo[r][g][b], obj_histo[r][g][b]);
				}
			}
		}
	}

	//compute flow from seeded histograms
	int flow = maxflow(draw_function);
	
	//color-e reestimation of histograms
	if (mode == COLOR_E) {
		//copy of m_labeling from previous iteration
		Table2D<Label> prev_labels;
					
		unsigned	//# pixels in image
					num_pixels = im.getHeight() * im.getWidth(),
					//number of pixel labels that changed this iteration
					changes_made = num_pixels,
					//new seed counts
					obj_seeds = 0,
					bkg_seeds = 0;

		//new set of histograms
		double	obj_histo[NUM_BINS][NUM_BINS][NUM_BINS] = { 0 },
				bkg_histo[NUM_BINS][NUM_BINS][NUM_BINS] = { 0 };

		//iterate until less than num_pixels * threshhold changes were made
		//so if threshhold = 0.0001, 0.01% of pixels need to change to keep iterating
		while (changes_made > num_pixels * BCD_THRESHHOLD) {
			changes_made = 0;
			prev_labels = m_labeling;

			//reset tlinks from unseeded pixels (undoing previous)
			for (unsigned x = 0; x < im.getWidth(); x++) {
				for (unsigned y = 0; y < im.getHeight(); y++) {
					Point p = { (int)x, (int)y };
					if (m_seeds[p] == NONE) {
						set_trcap(to_node(x,y), 0);
					}
				}
			}

			//build histogram from intensities of labels and seeds
			for (unsigned x = 0; x < im.getWidth(); x++) {
				for (unsigned y = 0; y < im.getHeight(); y++) {
					Point p = { (int)x, (int)y };
					//place each component into a bin
					int r = im[x][y].r / BIN_SIZE,
						g = im[x][y].g / BIN_SIZE,
						b = im[x][y].b / BIN_SIZE;

					//if the pixel currently belongs to the object (whether because of seed or previous label), put it in that rgb bin
					if (m_labeling[p] == OBJ || m_seeds[p] == OBJ) {
						obj_seeds++;
						obj_histo[r][g][b]++;
					}
					else if (m_labeling[p] == BKG || m_seeds[p] == BKG) {
						bkg_seeds++;
						bkg_histo[r][g][b]++;
					}
				}
			}

			//for each item in the histograms, use the intensities to calculate:
			//-lnPr(I | theta)
			for (unsigned i = 0; i < NUM_BINS; i++) {
				for (unsigned j = 0; j < NUM_BINS; j++) {
					for (unsigned k = 0; k < NUM_BINS; k++) {
						//log = ln
						//number of items in that bin / number of seeds
						//gives probability 
						double result = -log(obj_histo[i][j][k] / obj_seeds);
						//correct for max value if necessary
						if (result > INFTY)
							result = INFTY;
						obj_histo[i][j][k] = m_r * result;

						//same process for background seeds
						result = -log(bkg_histo[i][j][k] / bkg_seeds);
						if (result > INFTY)
							result = INFTY;
						bkg_histo[i][j][k] = m_r * result;
					}
				}
			}

			//calculate t-link weights for unseeded pixels
			for (unsigned x = 0; x < im.getWidth(); x++) {
				for (unsigned y = 0; y < im.getHeight(); y++) {
					Point p = { (int)x, (int)y };
					//ignore seeds
					if (m_seeds[p] == NONE) {
						int r = im[x][y].r / BIN_SIZE,
							g = im[x][y].g / BIN_SIZE,
							b = im[x][y].b / BIN_SIZE;

						add_tweights(p, bkg_histo[r][g][b], obj_histo[r][g][b]);
					}
				}
			}
			
			//update flow, labelling with new tlinks 
			flow = maxflow(draw_function);
			setLabeling();

			//count changed labels
			for (unsigned x = 0; x < im.getWidth(); x++) {
				for (unsigned y = 0; y < im.getHeight(); y++) {
					Point p = { (int)x, (int)y };
					//ignore seeds
					if (prev_labels[p] != m_labeling[p]) {
						changes_made++;
					}
				}
			}
		}
	}
	
	setLabeling();
	return flow;
}


// overwrites the base class function to allow visualization
// (e.g. calls specified "draw_function" to show each flow augmentation)
int Graph2D::maxflow( void (*draw_function)() ) 
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
	while ( 1 )
	{
		// test_consistency(current_node);

		if ((i=current_node))
		{
			i -> next = NULL; /* remove active flag */
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
			for (a=i->first; a; a=a->next)
			if (a->r_cap)
			{
				j = a -> head;
				if (!j->parent)
				{
					j -> is_sink = 0;
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
					set_active(j);
					add_to_changed_list(j);
				}
				else if (j->is_sink) break;
				else if (j->TS <= i->TS &&
				         j->DIST > i->DIST)
				{
					/* heuristic - trying to make the distance from j to the source shorter */
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
				}
			}
		}
		else
		{
			/* grow sink tree */
			for (a=i->first; a; a=a->next)
			if (a->sister->r_cap)
			{
				j = a -> head;
				if (!j->parent)
				{
					j -> is_sink = 1;
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
					set_active(j);
					add_to_changed_list(j);
				}
				else if (!j->is_sink) { a = a -> sister; break; }
				else if (j->TS <= i->TS &&
				         j->DIST > i->DIST)
				{
					/* heuristic - trying to make the distance from j to the sink shorter */
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
				}
			}
		}

		TIME ++;

		if (a)
		{
			i -> next = i; /* set active flag */
			current_node = i;

			// the line below can be uncommented to visualize current SOURCE and SINK trees
			//... if (draw_function) {setLabeling(); draw_function(); Pause(1);}

			/* augmentation */
			augment(a); // uses overwritten function of the base class (see code below)
			            // that keeps track of flow in table m_nodeFlow
			if (draw_function) {draw_function(); Pause(1);}
			/* augmentation end */

			/* adoption */
			while ((np=orphan_first))
			{
				np_next = np -> next;
				np -> next = NULL;

				while ((np=orphan_first))
				{
					orphan_first = np -> next;
					i = np -> ptr;
					nodeptr_block -> Delete(np);
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

	maxflow_iteration ++;
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
	bottleneck = middle_arc -> r_cap;
	for (i=middle_arc->sister->head; ; i=a->head)
	{
		a = i -> parent;
		if (a == TERMINAL) break;
		if (bottleneck > a->sister->r_cap) bottleneck = a -> sister -> r_cap;
	}
	if (bottleneck > i->tr_cap) bottleneck = i -> tr_cap;
	/* 1b - the sink tree */
	for (i=middle_arc->head; ; i=a->head)
	{
		a = i -> parent;
		if (a == TERMINAL) break;
		if (bottleneck > a->r_cap) bottleneck = a -> r_cap;
	}
	if (bottleneck > - i->tr_cap) bottleneck = - i -> tr_cap;


	/* 2. Augmenting */
	/* 2a - the source tree */
	middle_arc -> sister -> r_cap += bottleneck;
	middle_arc -> r_cap -= bottleneck;
	for (i=middle_arc->sister->head; ; i=a->head)
	{
		m_nodeFlow[to_point((node_id)(i-nodes))] = 1;
		a = i -> parent;
		if (a == TERMINAL) break;
		a -> r_cap += bottleneck;
		a -> sister -> r_cap -= bottleneck;
		if (!a->sister->r_cap)
		{
			set_orphan_front(i); // add i to the beginning of the adoption list
		}
	}
	i -> tr_cap -= bottleneck;
	if (!i->tr_cap)
	{
		set_orphan_front(i); // add i to the beginning of the adoption list
	}
	/* 2b - the sink tree */
	for (i=middle_arc->head; ; i=a->head)
	{
		m_nodeFlow[to_point((node_id)(i-nodes))] = 1;
		a = i -> parent;
		if (a == TERMINAL) break;
		a -> sister -> r_cap += bottleneck;
		a -> r_cap -= bottleneck;
		if (!a->r_cap)
		{
			set_orphan_front(i); // add i to the beginning of the adoption list
		}
	}
	i -> tr_cap += bottleneck;
	if (!i->tr_cap)
	{
		set_orphan_front(i); // add i to the beginning of the adoption list
	}


	flow += bottleneck;
}
