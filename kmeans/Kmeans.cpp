#include "cs1037lib-time.h" // for basic timing/pausing
#include "Kmeans.h"
using namespace std;


FeaturesGrid::FeaturesGrid()
: m_seeds()
, m_labeling()
, m_features()
, m_dim(0)
{}

void FeaturesGrid::reset(Table2D<RGB> & im, FEATURE_TYPE ft, float w) 
{
	cout << "resetting data... " << endl;
	int height = im.getHeight(), width =  im.getWidth();
	Point p;
	m_seeds.reset(width,height,NO_LABEL); 
	m_labeling.reset(width,height,NO_LABEL); // NOTE: width and height of m_labeling table 
	                                         // are used in functions to_index() and to_Point(), 
	m_dim=0;
	m_means.clear();
	m_features.clear();      // deleting old features
	m_features.resize(width*height); // creating a new array of feature vectors

	if (ft == color || ft == colorXY) {
		m_dim+=3;  // adding color features
		for (int y=0; y<height; y++) for (int x=0; x<width; x++) { 
			pix_id n = to_index(x,y);
			m_features[n].push_back((double) im[x][y].r);
			m_features[n].push_back((double) im[x][y].g);
			m_features[n].push_back((double) im[x][y].b);
		}
	}
	if (ft == colorXY) {
		m_dim+=2;  // adding location features (XY)
		for (int y=0; y<height; y++) for (int x=0; x<width; x++) { 
			pix_id n = to_index(x,y);
			m_features[n].push_back((double) w*x);
			m_features[n].push_back((double) w*y);
		}
	}

	cout << "done" << endl;
}

void FeaturesGrid::addFeature(Table2D<double> & im)
{
	m_dim++; // adding extra feature (increasing dimensionality of feature vectors
	int height = im.getHeight(), width =  im.getWidth();
	for (int y=0; y<height; y++) for (int x=0; x<width; x++) { 
		pix_id n = to_index(x,y);
		m_features[n].push_back(im[x][y]);
	}

}

// GUI calls this function when a user left- or right-clicks on an image pixel
void FeaturesGrid::addSeed(Point& p, Label seed_type) 
{
	if (!m_seeds.pointIn(p)) return;
	Label current_constraint = m_seeds[p];
	if (current_constraint==seed_type) return;
	m_seeds[p]=seed_type;
}

inline Label FeaturesGrid::what_label(Point& p)
{
	return m_labeling[p];
}

//% change in energy that must be achieved to continue iterating
//this solves the problem of some of the larger images taking a very long time to process
//while waiting until 0 changes are made
//make this number larger to speed the program up
#define THRESHOLD 0.00001
//1 for verbose output, 0 otherwise
#define VERBOSE 1

int FeaturesGrid::Kmeans(unsigned k) {
	init_means(k); 
	int iter = 0; // iteration counter
	cout << "Computing k-means for data..." << endl;	

	// WRITE YOUR CODE HERE TO SET m_labeling with Labels between 0 and k-1

	if(VERBOSE)
		cout << "Threshold to stop iterating is " << THRESHOLD * 100 << "% of a change in energy" << endl;
	
	//store energy, old energy for computing differences
	double energy, prev_energy;
	//flag for the first time through
	bool first = true;
	//kmeans iteration loop
	do {
		//reset the means, the old ones are discarded
		for (unsigned i = 0; i < k; i++) {
			for (unsigned j = 0; j < m_dim; j++) {
				m_means[i][j] = 0;
			}
		}

		//recompute means
		//stores the number of values in each cluster, since division is done in a different step
		vector<int> mean_sizes;
		mean_sizes.assign(k, 0);
		//loop through pixels and keep running sums
		for (unsigned i = 0; i < m_features.size(); i++) {
			//label of the current pixel
			Label l = what_label(to_point(i));
			//one more point is in cluster l
			mean_sizes[l]++;
			//loop through current pixel's data and add values to means
			for (unsigned j = 0; j < m_dim; j++) {
				m_means[l][j] += m_features[i][j];
			}
		}
	
		//divide each of the m_means by the cluster size to get cluster means
		for (unsigned i = 0; i < k; i++) {
			for (unsigned j = 0; j < m_dim; j++) {
				if(mean_sizes[i] != 0)
					m_means[i][j] /= mean_sizes[i];
			}
		}
		mean_sizes.clear();
		
		//every time but the first
		if (!first)
			prev_energy = energy;

		//assign features to nearest mean and get the number of changes made
		energy = assignFeaturesToNearestMean(k);
		//first time case
		if (first) {
			//+1 so the loop keeps executing after first iteration
			prev_energy = energy + THRESHOLD*energy + 1;
			first = false;
		}

		if (VERBOSE) {
			printMeans("Current", k);
		}

		iter++;
		cout << "Finished iteration " << iter << endl << endl;

		//while significant changes are being made
	} while (prev_energy - energy > THRESHOLD*energy);

	if (VERBOSE) {
		printMeans("Final", k);
	}

	return iter;
}

void FeaturesGrid::init_means(unsigned k) {
	m_means.clear();
	m_means.resize(k);

	for (Label ln = 0; ln < k; ln++) {
		for (unsigned i = 0; i < m_dim; i++) {
			m_means[ln].push_back(0.0); // setting to zero all mean vector components
		}
	}
	
	// WRITE YOUR CODE FOR INITIALIZING SEEDS (RANDOMLY OR FROM SEEDS IF ANY)

	vector<int> mean_sizes;
	mean_sizes.assign(k, 0);
	//loop through pixels and keep running sums for averaging
	for (unsigned i = 0; i < m_features.size(); i++) {
		//label of the seeded pixel
		Label l = m_seeds[to_point(i)];
		//one more point is in cluster l
		//make sure 0 <= l < k to validate input
		if (l >= 0 && l < k) {
			mean_sizes[l]++;
			//loop through current pixel's data and add values to means
			for (unsigned j = 0; j < m_dim; j++) {
				m_means[l][j] += m_features[i][j];
			}
		}
	}

	//divide each of the m_means by the cluster size to get cluster means
	for (unsigned i = 0; i < k; i++) {
		for (unsigned j = 0; j < m_dim; j++) {
			m_means[i][j] /= mean_sizes[i];
		}
	}

	//if not enough/no seeds were given, initialize randomly
	srand(time(NULL));
	for (unsigned i = 0; i < k; i++) {
		//don't overwrite seeded means
		if (mean_sizes[i] == 0) {
			vector<double> feature = m_features[rand() % m_features.size()];
			for (unsigned j = 0; j < m_dim; j++) {
				m_means[i][j] = feature[j];
			}
		}
	}

	//print out init means
	if (VERBOSE) {
		printMeans("Initial", k);
	}

	assignFeaturesToNearestMean(k);
}

//assigns each feature to its nearest mean
//returns the object function energy
double FeaturesGrid::assignFeaturesToNearestMean(unsigned k) {
	//objective function energy (sum of squared differences)
	//return value
	double energy = 0;

	//loop through all features
	for (unsigned i = 0; i < m_features.size(); i++) {
		//current feature
		vector<double> feature = m_features[i];
		//nearest mean to current feature
		unsigned nearest_mean = 0;
		//distance from nearest mean to feature
		double nearest_mean_distance = dist2(m_means[nearest_mean], feature);
		//start at 1, already calculated for 0
		for (unsigned j = 1; j < k; j++) {
			//calculate the distance to the current mean
			double distance = dist2(m_means[j], feature);
			//if it's smaller, this is the new nearest mean
			if (distance < nearest_mean_distance) {
				nearest_mean_distance = distance;
				nearest_mean = j;
			}
		}
		//track the sum of squared distances
		energy += nearest_mean_distance;
		//if there was a closer mean, set the new label
		Point p = to_point(i);
		if (what_label(p) != nearest_mean) {
			m_labeling[p] = nearest_mean;
		}
	}
	if (VERBOSE) {
		cout << "Energy: " << energy << endl;
	}

	return energy;
}

//prints all the means in m_means in a readable form
//called for initial, current, final means (specified in type)
void FeaturesGrid::printMeans(string type, unsigned k) {
	cout << type << " means: " << endl;
	for (unsigned i = 0; i < k; i++) {
		for (unsigned j = 0; j < m_dim; j++) {
			cout << m_means[i][j] << " ";
		}
		cout << endl;
	}
}