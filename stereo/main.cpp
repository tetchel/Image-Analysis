#include "cs1037lib-window.h" // for basic keyboard/mouse interface operations 
#include "cs1037lib-button.h" // for basic buttons, check-boxes, etc...
#include <iostream>     // for cout
#include "Cstr.h"       // convenient methods for generating C-style text strings
#include <vector>
#include "Basics2D.h"
#include "Table2D.h"
#include "Math2D.h"
#include "Image2D.h"
#include "Graph2D.h"

#define BIN_MAX 8

using namespace std;

// declaration of the main global variables/objects
Table2D<RGB> image; // image is "loaded" from a BMP file by function "image_load" below 
Table2D<RGB> image_R;
Graph2D graph; // a graph with max-flow/min-cut optimization
float s = 2;    // noise sensitivity parameter (sigma), default values for different images are set below
float r = 1;    // relative weight of the regularization term of the energy
int w = 1;		//window size - window is w pixels in each direction + 1 (for the central pixel). so w = 1, window is 3x3. w = 3, window is 7x7
int l = BIN_MAX;

			// declarations of global variables used for GUI controls/buttons/dropLists
const char* image_names[] = { "art" , "michael", "woman", "tsukuba" }; // an array of image file names
int im_index = 0;    // index of currently opened image (inital value)
int save_counter[] = { 0,0,0,0 }; // counter of saved results for each image above 
const char* mode_names[] = { "Window mode", "Scanline mode", "Global mode" }; // an array of mode names 
//const char* brush_names[] = { "Tiny brush", "Small brush", "Normal brush", "Large brush", "XLarge brush", "Box" }; // an array of brush names 
Mode mode = WINDOW; // stores current mode (index in the array 'mode_names')
bool view = false; // flag set by the check box
const int cp_height = 34; // height of "control panel" (area for buttons)
const int pad = 10; // width of extra "padding" around image (inside window)
char c = 'p'; // variable storing last pressed key;
int s_box; // handle for s-box
int r_box; // handle for r-box
int check_box_view; // handle for 'view' check-box
int button_reset;   // handle for 'reset' button

					// declarations of global functions used in GUI (see code below "main")
void image_load(int index); // call-back function for dropList selecting image file
void image_saveA();  // call-back function for button "Save A"
void image_saveB();  // call-back function for button "Save B"
void mode_set(int index);   // call-back function for dropList selecting mode 
void clear();  // call-back function for button "Clear"
void reset();  // call-back function for button "Reset"
void cut();    // call-back function for button "Cut", function called by mouse unclick and 'c'-key
void stereo();
void draw();
void view_set(bool v);  // call-back function for check box for "view" 
void w_set(const char* s_string);
void s_set(const char* s_string);  // call-back function for setting parameter "s" (contrast sensitivity) 
void r_set(const char* r_string);  // call-back function for setting parameter "r" (weight of the data/color term) 


int main()
{
	cout << "parameter w sets the window size" << endl;
	cout << "parameter l sets the number of layers (discrete depths)" << endl;
	cout << "press 's' to compute stereo image result" << endl;
	cout << "'q'-key quits the program " << endl << endl;

	// initializing buttons/dropLists and the window (using cs1037utils methods)
	int blank = CreateTextLabel(""); // adds grey "control panel" for buttons/dropLists, see "cs1037utils.h"
	SetControlPosition(blank, 0, 0); SetControlSize(blank, 1280, cp_height); // see "cs1037utils.h"
	int dropList_files = CreateDropList(4, image_names, im_index, image_load); // the last argument specifies the call-back function, see "cs1037utils.h"
	int dropList_modes = CreateDropList(3, mode_names, mode, mode_set); // the last argument specifies the call-back function, see "cs1037utils.h"
	//int dropList_brushes = CreateDropList(6, brush_names, br_index, brush_set); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_cut = CreateButton("Stereo", stereo); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_clear = CreateButton("Clear", clear); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_saveA = CreateButton("Save A", image_saveA); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_saveB = CreateButton("Save B", image_saveB); // the last argument specifies the call-back function, see "cs1037utils.h"
	check_box_view = CreateCheckBox("view", view, view_set); // see "cs1037utils.h"
	s_box = CreateTextBox(to_Cstr("w=" << w), w_set);
	r_box = CreateTextBox(to_Cstr("r=" << r), r_set);
	button_reset = CreateButton("Reset", reset); // the last argument specifies the call-back function, see "cs1037utils.h"
	SetWindowTitle("Stereo");      // see "cs1037utils.h"
	SetDrawAxis(pad, cp_height + pad, false); // sets window's "coordinate center" for GetMouseInput(x,y) and for all DrawXXX(x,y) functions in "cs1037utils" 
											  // we set it in the left corner below the "control panel" with buttons
											  // initializing the application
	image_load(im_index);
	SetWindowVisible(true); // see "cs1037utils.h"

							// while-loop processing keys/mouse interactions 
	while (!WasWindowClosed() && c != 'q') // WasWindowClosed() returns true when 'X'-box is clicked
	{
		if (GetKeyboardInput(&c)) // check keyboard
		{
			if (c == 's') stereo();
		}

		//int x, y, button;
		//if (GetMouseInput(&x, &y, &button)) // checks if there are mouse clicks or mouse moves
		//{
		//	Point mouse(x, y); // STORES PIXEL OF MOUSE CLICK
		//	if (button>0 && drag == 0) drag = button; // button 1 = left mouse click, 2 = right, 3 = middle
		//	if (drag>0) {
		//		stroke.push_back(mouse); // accumulating brush stroke points (mouse drag)
		//		if (br_index != gcBox) brush_fake(mouse, drag);  // and painting "fake" seeds over the image (for speed)
		//		if (br_index == gcBox && stroke.size()>1) set_gc_box(stroke[0], stroke.back());
		//	}
		//	if (button<0) {  // mouse released - adding hard "constraints" and computing segmentation
		//		if (drag>0) while (!stroke.empty()) { if (br_index != gcBox) brush_real(stroke.back(), drag); stroke.pop_back(); }
		//		cut();
		//		drag = 0;
		//	}
		//}
	}

	// deleting the controls
	DeleteControl(button_clear);    // see "cs1037utils.h"
	DeleteControl(button_cut);
	DeleteControl(button_reset);
	DeleteControl(dropList_files);
	DeleteControl(dropList_modes);
	//DeleteControl(dropList_brushes);
	DeleteControl(s_box);
	DeleteControl(check_box_view);
	DeleteControl(button_saveA);
	DeleteControl(button_saveB);
	return 0;
}

// call-back function for the 'mode' selection dropList 
// 'int' argument specifies the index of the 'mode' selected by the user among 'mode_names'
void mode_set(int index)
{
	mode = (Mode)index;
	cout << "mode is set to " << mode_names[mode] << endl;
	graph.reset(image, s, r);
	draw();
}

inline int max(int i, int j) {
	return i > j ? i : j;
}

// call-back function for the 'image file' selection dropList
// 'int' argument specifies the index of the 'file' selected by the user among 'image_names'
void image_load(int index)
{
	im_index = index;
	cout << "loading image file " << image_names[index] << "_L.bmp" << endl;
	image = loadImage<RGB>(to_Cstr(image_names[index] << "_L.bmp")); // global function defined in Image2D.h
	image_R = loadImage<RGB>(to_Cstr(image_names[index] << "_R.bmp")); // global function defined in Image2D.h
	int width = max(500, (int)image.getWidth()) + 2 * pad + 80;
	int height = max(100, (int)image.getHeight()) + 2 * pad + cp_height;
	SetWindowSize(width, height); // window height includes control panel ("cp")
	SetControlPosition(button_reset, image.getWidth() + pad + 5, cp_height + pad);
	SetControlPosition(s_box, image.getWidth() + pad + 5, cp_height + pad + 30);
	SetControlPosition(r_box, image.getWidth() + pad + 5, cp_height + pad + 60);
	SetControlPosition(check_box_view, image.getWidth() + pad + 5, cp_height + pad + 90);
	SetTextBoxString(s_box, "w=1"); // sets default value of parameter s for a given image
	SetTextBoxString(r_box, "l=2"); // sets default value of parameter r for a given image
	graph.reset(image, s, r);
	draw();
}

// call-back function for button "Clear"
void clear() {
	graph.reset(image, s, r);
	draw();
}

// call-back function for button "Reset"
void reset() {
	graph.reset(image, s, r, false); // clears current segmentation but keeps seeds or box - function in graphcuts.cpp
	draw();
}

void stereo() {
	cout << "Computing stereo image" << endl;

	graph.stereo(mode, image, image_R, w, l, ABSOLUTE, draw);
	draw();
}


// cut() computes optimal segmentation satisfying all current constraints
void cut()
{
	cout << "computing graph-cut segmentation..." << endl;
	int flow;
	if (view) flow = graph.compute_mincut(mode, image, draw); // calls "draw()" after each augmenting path 
	else      flow = graph.compute_mincut(mode, image);
	cout << " min-cut/max-flow value is " << flow << endl;
	draw();
}

// call-back function for check box for "view" check box 
void view_set(bool v) { view = v; draw(); }

// call-back function for setting parameter "w" 
// (window size) 
void w_set(const char* s_string) {
	float tmp;
	if (!sscanf_s(s_string, "w=%f", &tmp)) {
		//error
		//default for bad input
		w = 1;
		cout << "Bad input, input integers only" << endl;
	}
	else
		w = round(tmp);

	cout << "parameter 'w' is set to " << w << endl;
}

// call-back function for setting parameter "r" 
// (data/color tern weight) 
void r_set(const char* l_string) {
	float tmp;
	sscanf_s(l_string, "l=%f", &tmp);
	if (tmp >= BIN_MAX) {
		cout << "l value too large. Max is " << BIN_MAX << endl;
		l = BIN_MAX;
	}
	else if (l < 2) {
		cout << "l value too small. Min is 2" << endl;
		l = 2;
	}
		
	else
		l = round(tmp);
	cout << "parameter 'l' is set to " << l << endl;
}

// window drawing function
void draw()
{
	// Clear the window to white
	SetDrawColour(255, 255, 255); DrawRectangleFilled(-pad, -pad, 1280, 1024);

	if (!image.isEmpty()) drawImage(image); // draws image (object defined in graphcuts.cpp) using global function in Image2D.h (1st draw method there)
	else { SetDrawColour(255, 0, 0); DrawText(2, 2, "image was not found"); return; }

	// look-up tables specifying colors and transparancy for each possible integer value (FREE=0,OBJ=1,BKG=2) in "labeling"
	RGB colors[9] = { black, purple, blue, cyan, green, lime, yellow, orange, red };
	double transparancy1[9] = { 0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };
	double transparancy2[9] = { 0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };
	if (!graph.getLabeling().isEmpty()) drawImage(graph.getLabeling(), colors, transparancy1); // 4th draw() function in Image2D.h
	if (!graph.getSeeds().isEmpty())    drawImage(graph.getSeeds(), colors, transparancy2); // 4th draw() function in Image2D.h
	if (view) {
		RGB    colorsF[] = { black, green };
		double transparancyF[] = { 0.0,   0.4 };
		drawImage(graph.getFlow(), colorsF, transparancyF);
	}
	if (graph.getBox().size() == 2) { SetDrawColour(255, 255, 0); DrawRectangleOutline(graph.getBox()[0].x, graph.getBox()[0].y, graph.getBox()[1].x, graph.getBox()[1].y); }

}

// call-back function for button "SaveA"
void image_saveA()
{
	// FEEL FREE TO MODIFY THE CODE BELOW TO SAVE WHATEVER IMAGE YOU WOULD LIKE!!!
	// THIS VERSION SAVES A VIEW SIMILAR TO WHAT IS DRAWN ON THE SCREEN (in function draw() above)
	cout << "saving the results into image file... ";
	Table2D<RGB> tmp;

	// SAVES REGION MASK OVER THE ORIGINAL IMAGE (similar to what is drawn on the window)
	RGB    colors[3] = { black,  red,  blue };
	double transparancy1[3] = { 0,      0.2,   0.2 };
	double transparancy2[3] = { 0,      1.0,   1.0 };
	Table2D<RGB>    cMat1 = convert<RGB>(graph.getLabeling(), Palette(colors));   //3rd convert method in Table2D.h
	Table2D<double> aMat1 = convert<double>(graph.getLabeling(), Palette(transparancy1));
	tmp = cMat1%aMat1 + image % (1 - aMat1);  // painting labeling over image
	Table2D<RGB>    cMat2 = convert<RGB>(graph.getSeeds(), Palette(colors));   //3rd convert method in Table2D.h
	Table2D<double> aMat2 = convert<double>(graph.getSeeds(), Palette(transparancy2));
	tmp = cMat2%aMat2 + tmp % (1 - aMat2); // painting seeds
	if (view) {
		RGB    colorsF[] = { black, green };
		double transparancyF[] = { 0.0,   0.4 };
		Table2D<RGB>    cMat3 = convert<RGB>(graph.getFlow(), Palette(colorsF));   //3rd convert method in Table2D.h
		Table2D<double> aMat3 = convert<double>(graph.getFlow(), Palette(transparancyF));
		tmp = cMat3%aMat3 + tmp % (1 - aMat3); // painting flow
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

	// SAVES REGION MASK OVER THE ORIGINAL IMAGE (similar to what is drawn on the window)
	RGB    colors[3] = { black,  red,  blue };
	double transparancy[3] = { 0.8,     0,    0.8 };
	Table2D<RGB>    cMat = convert<RGB>(graph.getLabeling(), Palette(colors));   //3rd convert method in Table2D.h
	Table2D<double> aMat = convert<double>(graph.getLabeling(), Palette(transparancy));
	tmp = cMat%aMat + image % (1 - aMat);  // painting labeling over image

	save_counter[im_index]++;
	string name(to_Cstr("Results/result_" << image_names[im_index] << save_counter[im_index] << ".bmp"));
	saveImage(tmp, to_Cstr(name)); // saves to BMP file
	cout << name << endl; // prints the name of the saved .bmp file on the console
}
