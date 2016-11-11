#include "cs1037lib-window.h" // for basic keyboard/mouse interface operations 
#include "cs1037lib-button.h" // for basic buttons, check-boxes, etc...
#include <iostream>     // for cout
#include "Cstr.h"       // convenient methods for generating C-style text strings
#include <vector>
#include "Basics2D.h"
#include "Table2D.h"
#include "Math2D.h"
#include "Image2D.h"
#include "Kmeans.h"

using namespace std;

// declaration of the main global variables/objects
Table2D<RGB> image; // image is "loaded" from a BMP file by function "image_load" below 
FeaturesGrid grid_data; // data set: features of image pixels 
unsigned k;    // number of clusters
float w; // relative weight of XY component of features (vs RGB)

// declarations of global variables used for GUI controls/buttons/dropLists
const char*  k_values[]   = {"k=2" ,     "k=3"  ,   "k=6" ,    "k=4",   "k=3",  "k=3" };  // default values for "k" parameter for different images
const char* image_names[] = { "tools" , "hand", "roboSoccer", "rose", "bunny", "liver"}; // an array of image file names
int im_index = 0;    // index of currently opened image (inital value)
int save_counter[] = {0,0,0,0,0,0}; // counter of saved results for each image above 
const char* mode_names[]  = { "rgb mode", "rgbxy mode", "YOUR mode"}; // an array of mode names 
const char* brush_names[]  = { "Tiny brush", "Small brush", "Normal brush", "Large brush", "XLarge brush"}; // an array of brush names 
enum Mode {rgb=0, rgbxy=1, MYMODE=2}; 
enum Brush {Tiny=0, Small=1, Normal=2, Large=3, XLarge=4}; 
Mode mode=rgb; // stores current mode (index in the array 'mode_names')
Brush br_index=Normal; // stores current brush index (in the array 'brush_names')
vector<Point> brush[5]; // stores shifts for traversing pixels for different brushes 
int drag=0; // variable storing mouse dragging state
vector<Point> stroke; // vector for tracking brush strokes (mouse drags)
bool view = false; // flag set by the check box
const int cp_height = 34; // height of "control panel" (area for buttons)
const int pad = 10; // width of extra "padding" around image (inside window)
char c='p'; // variable storing last pressed key;
int k_box; // handle for k-box
int w_box; // handle for w-box
int check_box_view; // handle for 'view' check-box
int button_saveA;  // handle for 'saveA' button
int button_saveB;  // handle for 'saveB' button

// declarations of global functions used in GUI (see code below "main")
void image_load(int index); // call-back function for dropList selecting image file
void image_saveA();  // call-back function for button "Save A"
void image_saveB();  // call-back function for button "Save B"
void mode_set(int index);   // call-back function for dropList selecting mode 
void brush_set(int index);   // call-back function for dropList selecting brush 
void brush_real(Point click, int drag); // to add a "brush stroke" of seeds (hard constraints)
void brush_fake(Point click, int drag);   // paints brush stroke over the image
void brush_init(); // functions for initializing brushes
void clear();  // call-back function for button "Clear"
void cluster();    // call back for "K-means" button, also called by mouse unclick, 'ENTER', and 'c'-key
void draw(); 
void view_set(bool v);  // call-back function for check box for "view" 
void k_set(const char* s_string);  // call-back function for setting parameter "k" (number of clusters) 
void w_set(const char* s_string);  // call-back function for setting parameter "w" (relative weight of XY) 


int main()
{
	cout << "press 'Kmeans' or 'ENTER' to compute K-means clusters in RGB or RBGXY space" << endl;
	cout << "press any digit-key < k and use mouse clicks/drags to enter clusters' seeds" << endl;
	cout << "middle mouse clicks/drags delete seeds" << endl; 
	cout << "check 'view means' to see color means within each cluster" << endl;
	cout <<	"press 'Clear' or 'c'-key to delete current clustering (and seeds)" << endl; 
	cout << "'q'-key quits the program " << endl << endl; 

	  // initializing buttons/dropLists and the window (using cs1037utils methods)
	int blank = CreateTextLabel(""); // adds grey "control panel" for buttons/dropLists, see "cs1037utils.h"
    SetControlPosition(blank,0,0); SetControlSize(blank,1280,cp_height); // see "cs1037utils.h"
	int dropList_files = CreateDropList(6, image_names, im_index, image_load); // the last argument specifies the call-back function, see "cs1037utils.h"
	int dropList_modes = CreateDropList(3, mode_names, mode, mode_set); // the last argument specifies the call-back function, see "cs1037utils.h"
	int dropList_brushes = CreateDropList(5, brush_names, br_index, brush_set); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_Kmeans = CreateButton("Kmeans",cluster); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_clear = CreateButton("Clear",clear); // the last argument specifies the call-back function, see "cs1037utils.h"
	button_saveA = CreateButton("Save A",image_saveA); // the last argument specifies the call-back function, see "cs1037utils.h"
	button_saveB = CreateButton("Save B",image_saveB); // the last argument specifies the call-back function, see "cs1037utils.h"
	check_box_view = CreateCheckBox("view color means" , view, view_set); // see "cs1037utils.h"
	k_box = CreateTextBox(to_Cstr("k=" << k), k_set); 
	w_box = CreateTextBox(to_Cstr("w=" << w), w_set); 
	SetWindowTitle("CS4487 hw1: K-means");      // see "cs1037utils.h"
    SetDrawAxis(pad,cp_height+pad,false); // sets window's "coordinate center" for GetMouseInput(x,y) and for all DrawXXX(x,y) functions in "cs1037utils" 
	                                      // we set it in the left corner below the "control panel" with buttons
	// initializing the application
	w = 1.0;
	image_load(im_index);
	brush_init();
	SetWindowVisible(true); // see "cs1037utils.h"

	  // while-loop processing keys/mouse interactions 
	while (!WasWindowClosed() && c!='q') // WasWindowClosed() returns true when 'X'-box is clicked
	{
		if (GetKeyboardInput(&c)) // check keyboard
		{  
			if ( c == char(13)) cluster();   // char(13) corresponds to "enter"
			else if (c == 'c') clear();
		}

		int x, y, button;
		if (GetMouseInput(&x, &y, &button)) // checks if there are mouse clicks or mouse moves
		{
			Point mouse(x,y); // STORES PIXEL OF MOUSE CLICK
			if (button>0 && drag==0) drag=button; // button 1 = left mouse click, 2 = right, 3 = middle
			if (drag>0) {
				stroke.push_back(mouse); // accumulating brush stroke points (mouse drag)
				brush_fake(mouse,drag);  // and painting "fake" seeds over the image (for speed)
			}  			                                                                
			if (button<0) {  // mouse released - adding hard "constraints" and computing segmentation
				if (drag>0) while (!stroke.empty()) {brush_real(stroke.back(),drag); stroke.pop_back();}
				//cluster(); 
				drag = 0; 
			}	
		}
	}

	  // deleting the controls
	DeleteControl(button_clear);    // see "cs1037utils.h"
	DeleteControl(button_Kmeans);
	DeleteControl(dropList_files);
	DeleteControl(dropList_modes);     
	DeleteControl(dropList_brushes);     
	DeleteControl(k_box);
	DeleteControl(check_box_view);
	DeleteControl(button_saveA);
	DeleteControl(button_saveB);
	return 0;
}

// call-back function for the 'mode' selection dropList 
// 'int' argument specifies the index of the 'mode' selected by the user among 'mode_names'
void mode_set(int index)
{
	mode = (Mode) index;
	cout << "mode is set to " << mode_names[mode] << endl;
	grid_data.reset(image,(FEATURE_TYPE) mode,w);
	draw();
}

// call-back function for the 'image file' selection dropList
// 'int' argument specifies the index of the 'file' selected by the user among 'image_names'
void image_load(int index) 
{
	im_index = index;
	cout << "loading image file " << image_names[index] << ".bmp" << endl;
	image = loadImage<RGB>(to_Cstr(image_names[index] << ".bmp")); // global function defined in Image2D.h
	int width = (int)image.getWidth() + 2 * pad + 80 > 500 ? (int)image.getWidth() + 2 * pad + 80 : 500; //max(500, (int)image.getWidth()) + 2 * pad + 80;
	int height = (int)image.getHeight() + 2 * pad + 2 * cp_height > 100 ? (int)image.getHeight() + 2 * pad + 2 * cp_height : 100; // max(100, (int)image.getHeight()) + 2 * pad + 2 * cp_height;
	SetWindowSize(width,height); // window height includes control panel ("cp")
    SetControlPosition(    k_box,     image.getWidth()+pad+5, cp_height+pad+30);
    SetControlPosition(    w_box,     image.getWidth()+pad+5, cp_height+pad+60);
    SetControlPosition(check_box_view,image.getWidth()+pad+5, cp_height+pad+90);
    SetControlPosition(button_saveA,  pad+5, height-cp_height);
    SetControlPosition(button_saveB,  pad+70, height-cp_height);
    SetTextBoxString(k_box,k_values[index]); // sets default value of parameter k for a given image
    SetTextBoxString(w_box,to_Cstr("w=" << w)); // sets value of parameter w
	grid_data.reset(image,(FEATURE_TYPE) mode,w);
	draw();
}

// call-back function for button "Clear"
void clear() { 
	grid_data.reset(image,(FEATURE_TYPE) mode,w);
	draw();
}

// cut() computes optimal segmentation satisfying all current constraints
void cluster()
{
	cout << " number of iterations is " << grid_data.Kmeans(k) << endl;
	draw();
}

// call-back function for check box for "view" check box 
void view_set(bool v) {view=v; draw();}  

// call-back function for setting parameter "k" (number of clusters) 
void k_set(const char* k_string) {
	unsigned temp_k;
	sscanf_s(k_string,"k=%d",&temp_k);
	if (temp_k<=12 && temp_k>1 && temp_k!=k) {k=(unsigned)temp_k; cout << "parameter 'k' is set to " << k << endl;}
	else cout << "parameter 'k' should be in the range 2,3,...,12, 'k' remains " << k << endl;
	SetTextBoxString(k_box,to_Cstr("k=" << k));
}

// call-back function for setting parameter "w" (weight of XY part of features) 
void w_set(const char* w_string) {
	float temp_w;
	sscanf_s(w_string,"w=%f",&temp_w);
	if (temp_w>=0.0 && temp_w!=w) {w=temp_w; cout << "parameter 'w' is set to " << w << endl; grid_data.reset(image,(FEATURE_TYPE) mode,w);}
	SetTextBoxString(w_box,to_Cstr("w=" << w));
}


// call-back function for the 'brush' selection dropList 
// 'int' argument specifies the index of the 'brush' selected by the user among 'brush_names'
void brush_set(int index)
{
	br_index = (Brush) index;
	cout << "brush is set to " << brush_names[br_index] << endl;
}

// function for adding a "brush stroke" of seeds
void brush_real(Point click, int drag)
{
	vector<Point>::iterator i, start = brush[br_index].begin(), end = brush[br_index].end();  
	if      ((drag==1 || drag==2) && c>='0' && c<='9') {
		unsigned t;
		sscanf_s(to_Cstr(c),"%d",&t);
		if (t<k) for (i=start; i<end; ++i) grid_data.addSeed(click+(*i),t); // function addSeed() is defined in FeaturesGrid.h
	}
	else if (drag==3) for (i=start; i<end; ++i) grid_data.addSeed(click+(*i),NO_LABEL);
}

// function for painting "brush stroke" over image
void brush_fake(Point click, int drag)
{
	if      (drag==1) SetDrawColour(255,178,178);
	else if (drag==2) SetDrawColour(178,178,255);
	else if (drag==3) SetDrawColour(204,204,204);
	else return;
	int r = 4*br_index;
	DrawEllipseOutline(click.x,click.y,r,r); // function from cs1037util.h
}

// functions for initializing different brush "strokes"
void brush_init() {
	for (int i=0; i<=XLarge; i++) {
		int t = 16*i*i;
		int s = 1+((int)sqrt((float)t));
		for (int x=-s; x<=s; x++) for (int y=-s; y<=s; y++) {
			if ((x*x+y*y)<=t) brush[i].push_back(Point(x,y));
	    }
	}
}

// window drawing function
void draw()
{ 
	// Clear the window to white
	SetDrawColour(255,255,255); DrawRectangleFilled(-pad,-pad,1280,1024);
	
	if (image.isEmpty()) {SetDrawColour(255, 0, 0); DrawText(2,2,"image was not found"); return;}
	else if (!view)	drawImage(image); // draws image (object defined in graphcuts.cpp) using global function in Image2D.h (1st draw method there)

	// look-up tables specifying colors and transparancy for 20 possible integer value (0,1,2,...,11,NO_LABEL=12) in "labeling"
	RGB    colors[13]        = { red,  blue, green, cyan, yellow, magenta, teal, navy, lime, maroon, purple, olive, black};
	double transparancy1[13] = { 0.4,   0.4,   0.4,  0.4,    0.4,     0.4,  0.4,  0.4,  0.4,    0.4,    0.4,   0.4,   0  };
	double transparancy2[13] = { 1.0,   1.0,   1.0,  1.0,    1.0,     1.0,  1.0,  1.0,  1.0,    1.0,    1.0,   1.0,   0  };
	const vector<vector<double>> means = grid_data.getMeans();
	if (view && !means.empty()) {
		for (unsigned t=0; t<k; t++) {
			transparancy1[t] = 1.0;
			colors[t]        = RGB((unsigned char) means[t][0],(unsigned char) means[t][1], (unsigned char) means[t][2]);
		}
		for (unsigned t=0; t<13; t++) transparancy2[t] = 0.0; 
	}

	if (!grid_data.getLabeling().isEmpty()) drawImage(grid_data.getLabeling(),colors,transparancy1); // 4th draw() function in Image2D.h
	if (!grid_data.getSeeds().isEmpty())    drawImage(grid_data.getSeeds(),colors,transparancy2); // 4th draw() function in Image2D.h
}

// call-back function for button "SaveA"
void image_saveA() 
{
	// FEEL FREE TO MODIFY THE CODE BELOW TO SAVE WHATEVER IMAGE YOU WOULD LIKE!!!
	// THIS VERSION SAVES A VIEW SIMILAR TO WHAT IS DRAWN ON THE SCREEN (in function draw() above)
	cout << "saving the results into image file... ";
	Table2D<RGB> tmp;

	if (mode==rgb || mode ==rgbxy) {    // SAVES REGION MASK OVER THE ORIGINAL IMAGE (similar to what is drawn on the window)
		RGB    colors[13]        = { red,  blue, green, cyan, yellow, magenta, teal, navy, lime, maroon, purple, olive, black};
		double transparancy1[13] = { 0.4,   0.4,   0.4,  0.4,    0.4,     0.4,  0.4,  0.4,  0.4,    0.4,    0.4,   0.4,   0  };
		double transparancy2[13] = { 1.0,   1.0,   1.0,  1.0,    1.0,     1.0,  1.0,  1.0,  1.0,    1.0,    1.0,   1.0,   0  };
		Table2D<RGB>    cMat1 = convert<RGB>(grid_data.getLabeling(),Palette(colors));   //3rd convert method in Table2D.h
		Table2D<double> aMat1 = convert<double>(grid_data.getLabeling(),Palette(transparancy1)); 
		tmp = cMat1%aMat1 + image%(1-aMat1);  // painting labeling over image
		Table2D<RGB>    cMat2 = convert<RGB>(grid_data.getSeeds(),Palette(colors));   //3rd convert method in Table2D.h
		Table2D<double> aMat2 = convert<double>(grid_data.getSeeds(),Palette(transparancy2)); 
		tmp = cMat2%aMat2 + tmp%(1-aMat2); // painting seeds
	}
	
	save_counter[im_index]++;
	string name(to_Cstr("Results/result_" << image_names[im_index] << save_counter[im_index] << ".bmp"));
	saveImage(tmp, to_Cstr(name)); // saves to BMP file
	cout << name << endl; // prints the name of the saved .bmp file on the console
}

// call-back function for button "SaveB"
void image_saveB() 
{
	// FEEL FREE TO MODIFY THE CODE BELOW TO SAVE WHATEVER IMAGE YOU WOULD LIKE!!!
	// This version saves image with washed out background pixels
	cout << "saving the results into image file... ";
	Table2D<RGB> tmp;

	if (mode==rgb || mode ==rgbxy) {    // SAVES REGION MASK OVER THE ORIGINAL IMAGE (similar to what is drawn on the window)
		RGB    colors[13]; 
		double transparancy[13];
		const vector<vector<double>> means = grid_data.getMeans();
		if (means.empty()) { cout << "mo color 'means' are available in saveB" << endl; return;} 
		for (unsigned t=0; t<k; t++) {
			transparancy[t] = 1.0;
			colors[t]       = RGB((unsigned char) means[t][0],(unsigned char) means[t][1], (unsigned char) means[t][2]);
		}
		Table2D<RGB>    cMat = convert<RGB>(grid_data.getLabeling(),Palette(colors));   //3rd convert method in Table2D.h
		Table2D<double> aMat = convert<double>(grid_data.getLabeling(),Palette(transparancy)); 
		tmp = cMat%aMat + image%(1-aMat);  // painting labeling over image
	}
	
	save_counter[im_index]++;
	string name(to_Cstr("Results/result_" << image_names[im_index] << save_counter[im_index] << ".bmp"));
	saveImage(tmp, to_Cstr(name)); // saves to BMP file
	cout << name << endl; // prints the name of the saved .bmp file on the console
}

