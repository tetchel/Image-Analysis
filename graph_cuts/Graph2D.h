/* Graph2D.h */

//	class Graph2D is derived from "Graph" (BK maxflow library) to suit the following goals:
//   - specialization for integer edge weights (n-links and t-links)
//	 - specialization to 4-connected 2D grid graphs constructed from 2D images 
//	 - modification of interface allowing access to graph nodes as pixels (Points)
//	 - allowing visualization of augmenting paths (primarily for educational purposes)

#pragma once
#include "maxflow/graph.h"
#include "Basics2D.h"
#include "Table2D.h"
#include "Image2D.h"
#include <vector>

using namespace std;

enum Label {NONE=0, OBJ=1, BKG=2};
enum Mode { EDGE = 0, COLOR_F = 1, COLOR_E = 2 };


class Graph2D : public Graph<int,int,int>   // NOTE: one can use all base-class functions too
{
public:
	// constructors/reset
	Graph2D();
	void reset(Table2D<RGB> & im, double sigma, double r, bool eraze_seeds=true);

	void addSeed(Point& p, Label seed_type); 
	void setBox(Point& tl, Point& br);

	inline const vector<Point>& getBox() const {return m_box;}
	inline const Table2D<Label>& getSeeds() const {return m_seeds;}
	inline const Table2D<Label>& getLabeling() const {return m_labeling;}
	inline const Table2D<int>& getFlow() const {return m_nodeFlow;}

	// "what_label" returns Label of the node, depending on the terminal this node is connected to.
	// NOTE: unlike "what_segment" function of the base class, "what_label" returns NONE
	// for graph nodes that are disconnected from both terminals (SOURCE or SINK)
	// ("what_segment" arbitrarily labels such nodes with some default value, SOURCE or SINK)
	inline Label what_label(Point& p);

	// overwriting base-class functions "add_tweights" to allow access to nodes as pixels (Points)
	void add_tweights(Point& p, int cap_source, int cap_sink) {Graph<int,int,int> :: add_tweights(to_node(p),cap_source,cap_sink);}

	// function that computes min-cut on a current graph. If "draw_function" is specified,
	// it is called after each augmenting path of the maxflow algorithm. For example, 
	// such "draw_function" can call getFlow() to visualize the current flow on the graph. 
	// Upon termination, function "compute_mincut" sets node labeling table according to the 
	// minimum cut obtained for the current graph and returns the minimum cut (maximum flow) value.
	int compute_mincut(Mode, Table2D<RGB>, void (*draw_function)() = NULL);

private:

	// functions for converting node indeces to Points and vice versa.
	inline Point to_point(node_id node) { return Point(node%m_nodeFlow.getWidth(),node/m_nodeFlow.getWidth());}  
	inline node_id to_node(Point p) {return p.x+p.y*m_nodeFlow.getWidth();}
	inline node_id to_node(int x, int y) {return x+y*m_nodeFlow.getWidth();}

	void setLabeling(); // sets up m_labeling array of pixel labels (called in the end of "compute_mincut" above)

	vector<Point>  m_box;  // (if non empty), can be used as top-left and right-bottom corners of the box for GrabCut;
	Table2D<Label> m_seeds;  // look-up table for storing seeds (hard constraints)
	Table2D<Label> m_labeling; // labeling of image pixels
	Table2D<int> m_nodeFlow; // 2D array to store flow passing through grid nodes.

	double m_r; // relative weight of the data/color term of the energy 

	// overwriting 2 base-class functions ONLY to allow visualization of augmenting paths (flow). 
	// Function "maxflow" below calls any specified "draw function" that can  getFlow() to 
	// access node-flow array in order to display it. Function "augment" below (in addition to 
	// everything done by the base-class function "augment") saves node flow in 2D array m_nodeFlow.
	// In fact, one can completely remove these "augment" and "maxflow" below from class Graph2D 
	// if visualization of augmenting paths is not needed. In this case, call base-class function
	// maxflow() inside the code for   compute_mincut()  function in Graph2D.cpp
	int maxflow(void (*draw_function)());
	void augment(arc *middle_arc);

};
