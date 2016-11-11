/* Kmeans.h */

//	class FeaturesGrid suits the following goals:
//   - stores N-dimentional image features that can be queried as pixels (Points) in 2D image
//	 - can compute clusters using K-means allowing partial labeling of points via seeds

#pragma once
#include "Basics2D.h"
#include "Table2D.h"
#include "Image2D.h"
#include <iostream>     // for cout
#include <vector>
#include <time.h>
#include <string>

using namespace std;

enum FEATURE_TYPE {color=0, colorXY=1};  // this is for features activated by constructor and reset functions)
                                         // should correspond to "enum Mode" in main

const unsigned NO_LABEL = 12; // note: k<=12 (number of clusters)
typedef unsigned Label;  // index of cluster (0,1,2,...,k-1), NO_LABEL = 12 is a special value (see above)
typedef unsigned pix_id; // internal (private) indices of features 


class FeaturesGrid
{
public:
	// constructors/reset
	FeaturesGrid();
	void   reset(Table2D<RGB> & im, FEATURE_TYPE ft = color, float w=0);

	void addSeed(Point& p, Label seed_type); 

	void addFeature(Table2D<double> & im);	// allows to add any feature type (e.g. im could be output of any filter)
											// image im should have the same width and height and the original image
											// used to create FeaturesGrid (inside constructor or reset function)

	inline const Table2D<Label>& getSeeds() const {return m_seeds;}			// returns the current set of seeds 
	inline const Table2D<Label>& getLabeling() const {return m_labeling;}	// returns current labeling (clustering)
	inline const vector<vector<double>>& getMeans() const {return m_means;} // returns current cluster means)

	// "what_label" returns current Label of the node (cluster index)
	inline Label what_label(Point& p);

	inline unsigned getDim() {return m_dim;}

	// function that computes (or updates) clustering (labeling) of features points respecting seeds (if any). 
	int Kmeans(unsigned k); // ignores seed points with  Label>k or Label=NO_LABEL
	                        // returns the number of iterations to convergence

private:

	// functions for converting node indeces to Points and vice versa.
	inline Point to_point(pix_id index) { return Point(index%m_labeling.getWidth(),index/m_labeling.getWidth());}  
	inline pix_id to_index(Point p) {return p.x+p.y*m_labeling.getWidth();}
	inline pix_id to_index(int x, int y) {return x+y*m_labeling.getWidth();}
	void init_means(unsigned k); // used to compute initial "means" in kmeans
	double assignFeaturesToNearestMean(unsigned);
	void printMeans(string, unsigned);

	Table2D<Label> m_seeds;				// look-up table for storing seeds (partial labeling constraints)
	Table2D<Label> m_labeling;			// labeling of image pixels
	vector<vector<double>> m_features;	// internal array of N-dimentional features created from pixels intensities, colors, location, etc.
	unsigned m_dim;						// stores current dimensionality of feature vectors
	vector<vector<double>> m_means;		// cluster means

	// squared distance between two vectors of dimension m_dim
	inline double dist2(vector<double> &a, vector<double> &b) {
		double d=0.0; 
		for (unsigned i=0; i<m_dim; i++) d+=((a[i]-b[i])*(a[i]-b[i]));
		return d;
	}

};
