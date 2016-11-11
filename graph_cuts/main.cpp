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

using namespace std;

// declaration of the main global variables/objects
Table2D<RGB> image; // image is "loaded" from a BMP file by function "image_load" below 
Graph2D graph; // a graph with max-flow/min-cut optimization
float s;    // noise sensitivity parameter (sigma), default values for different images are set below
float r;    // relative weight of the regularization term of the energy

// declarations of global variables used for GUI controls/buttons/dropLists
const char*  s_values[]   = {"s=4.0" ,  "s=2.0"  , "s=5.0" ,  "s=8.0"  , "s=10.0", "s=4.0" }; // default values for "s" parameter for different images
const char*  r_values[]   = {"r=1.0" ,  "r=1.0"  , "r=1.0" ,  "r=1.0"  , "r=1.0", "r=1.0" }; // default values for "r" parameter for different images
const char* image_names[] = { "rose" , "hand", "canada", "bunny" , "model" , "liver" }; // an array of image file names
int im_index = 2;    // index of currently opened image (inital value)
int save_counter[] = {0,0,0,0,0,0}; // counter of saved results for each image above 
const char* mode_names[]  = { "EDGE mode", "COLOR_F mode", "COLOR_E mode"}; // an array of mode names 
const char* brush_names[]  = { "Tiny brush", "Small brush", "Normal brush", "Large brush", "XLarge brush", "Box"}; // an array of brush names 
enum Brush {Tiny=0, Small=1, Normal=2, Large=3, XLarge=4, gcBox=5}; 
Mode mode=EDGE; // stores current mode (index in the array 'mode_names')
Brush br_index=Normal; // stores current brush index (in the array 'brush_names')
vector<Point> brush[5]; // stores shifts for traversing pixels for different brushes 
int drag=0; // variable storing mouse dragging state
vector<Point> stroke; // vector for tracking brush strokes (mouse drags)
bool view = false; // flag set by the check box
const int cp_height = 34; // height of "control panel" (area for buttons)
const int pad = 10; // width of extra "padding" around image (inside window)
char c='p'; // variable storing last pressed key;
int s_box; // handle for s-box
int r_box; // handle for r-box
int check_box_view; // handle for 'view' check-box
int button_reset;   // handle for 'reset' button

// declarations of global functions used in GUI (see code below "main")
void image_load(int index); // call-back function for dropList selecting image file
void image_saveA();  // call-back function for button "Save A"
void image_saveB();  // call-back function for button "Save B"
void mode_set(int index);   // call-back function for dropList selecting mode 
void brush_set(int index);   // call-back function for dropList selecting brush 
void brush_real(Point click, int drag); // to add a "brush stroke" of seeds (hard constraints)
void brush_fake(Point click, int drag);   // paints brush stroke over the image
void set_gc_box(Point tl, Point br); // sets top_left (tl) and bottom_right (br) corners of the box for GrabCut 
void brush_init(); // functions for initializing brushes
void clear();  // call-back function for button "Clear"
void reset();  // call-back function for button "Reset"
void cut();    // call-back function for button "Cut", function called by mouse unclick and 'c'-key
void draw(); 
void view_set(bool v);  // call-back function for check box for "view" 
void s_set(const char* s_string);  // call-back function for setting parameter "s" (contrast sensitivity) 
void r_set(const char* r_string);  // call-back function for setting parameter "r" (weight of the data/color term) 


int main()
{
	cout << "button 'Clear' deletes current segmentation, seeds, and box" << endl; 
	cout << "button 'Reset' deletes segmentation, but keeps the seeds and/or box" << endl; 
	cout << "left/right clicks (and 'drags') enter object/background seeds in BRUSH mode" << endl;
	cout << "middle mouse clicks (and 'drags') delete seeds" << endl; 
	cout << "mouse 'drags' enter a box when in BOX mode (drop-list item in BRUSHes)" << endl;
	cout << "mouse 'unclicks' compute graph-cuts segmentation (also 'c'-key or 'Cut' button)" << endl; 
	cout << "parameter 's' sets 'sigma' in contrast-sensitive edge cost function w(dI)" << endl;
	cout << "parameter 'r' sets relative weight of the color appearance term of the energy" << endl;
	cout << "'q'-key quits the program " << endl << endl; 

	  // initializing buttons/dropLists and the window (using cs1037utils methods)
	int blank = CreateTextLabel(""); // adds grey "control panel" for buttons/dropLists, see "cs1037utils.h"
    SetControlPosition(blank,0,0); SetControlSize(blank,1280,cp_height); // see "cs1037utils.h"
	int dropList_files = CreateDropList(6, image_names, im_index, image_load); // the last argument specifies the call-back function, see "cs1037utils.h"
	int dropList_modes = CreateDropList(3, mode_names, mode, mode_set); // the last argument specifies the call-back function, see "cs1037utils.h"
	int dropList_brushes = CreateDropList(6, brush_names, br_index, brush_set); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_cut = CreateButton("Cut",cut); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_clear = CreateButton("Clear",clear); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_saveA = CreateButton("Save A",image_saveA); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_saveB = CreateButton("Save B",image_saveB); // the last argument specifies the call-back function, see "cs1037utils.h"
	check_box_view = CreateCheckBox("view" , view, view_set); // see "cs1037utils.h"
	s_box = CreateTextBox(to_Cstr("s=" << s), s_set); 
	r_box = CreateTextBox(to_Cstr("r=" << r), r_set); 
	button_reset = CreateButton("Reset",reset); // the last argument specifies the call-back function, see "cs1037utils.h"
	SetWindowTitle("Graph Cuts");      // see "cs1037utils.h"
    SetDrawAxis(pad,cp_height+pad,false); // sets window's "coordinate center" for GetMouseInput(x,y) and for all DrawXXX(x,y) functions in "cs1037utils" 
	                                      // we set it in the left corner below the "control panel" with buttons
	// initializing the application
	image_load(im_index);
	brush_init();
	SetWindowVisible(true); // see "cs1037utils.h"

	  // while-loop processing keys/mouse interactions 
	while (!WasWindowClosed() && c!='q') // WasWindowClosed() returns true when 'X'-box is clicked
	{
		if (GetKeyboardInput(&c)) // check keyboard
		{  
			if ( c == 'c') cut();
		}

		int x, y, button;
		if (GetMouseInput(&x, &y, &button)) // checks if there are mouse clicks or mouse moves
		{
			Point mouse(x,y); // STORES PIXEL OF MOUSE CLICK
			if (button>0 && drag==0) drag=button; // button 1 = left mouse click, 2 = right, 3 = middle
			if (drag>0) {
				stroke.push_back(mouse); // accumulating brush stroke points (mouse drag)
				if (br_index!=gcBox) brush_fake(mouse,drag);  // and painting "fake" seeds over the image (for speed)
				if (br_index==gcBox && stroke.size()>1) set_gc_box(stroke[0],stroke.back());
			}  			                                                                
			if (button<0) {  // mouse released - adding hard "constraints" and computing segmentation
				if (drag>0) while (!stroke.empty()) {if (br_index!=gcBox) brush_real(stroke.back(),drag); stroke.pop_back();}
				cut(); 
				drag = 0; 
			}	
		}
	}

	  // deleting the controls
	DeleteControl(button_clear);    // see "cs1037utils.h"
	DeleteControl(button_cut);
	DeleteControl(button_reset);
	DeleteControl(dropList_files);
	DeleteControl(dropList_modes);     
	DeleteControl(dropList_brushes);     
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
	mode = (Mode) index;
	cout << "mode is set to " << mode_names[mode] << endl;
	graph.reset(image,s,r);
	draw();
}

inline int max(int i, int j) {
	return i > j ?  i : j;
}

// call-back function for the 'image file' selection dropList
// 'int' argument specifies the index of the 'file' selected by the user among 'image_names'
void image_load(int index) 
{
	im_index = index;
	cout << "loading image file " << image_names[index] << ".bmp" << endl;
	image = loadImage<RGB>(to_Cstr(image_names[index] << ".bmp")); // global function defined in Image2D.h
	int width  = max(500,(int)image.getWidth()) + 2*pad + 80;
	int height = max(100,(int)image.getHeight())+ 2*pad + cp_height;
	SetWindowSize(width,height); // window height includes control panel ("cp")
    SetControlPosition(button_reset,image.getWidth()+pad+5, cp_height+pad);
    SetControlPosition(    s_box,     image.getWidth()+pad+5, cp_height+pad+30);
    SetControlPosition(    r_box,     image.getWidth()+pad+5, cp_height+pad+60);
    SetControlPosition(check_box_view,image.getWidth()+pad+5, cp_height+pad+90);
    SetTextBoxString(s_box,s_values[index]); // sets default value of parameter s for a given image
    SetTextBoxString(r_box,r_values[index]); // sets default value of parameter r for a given image
	graph.reset(image,s,r);
	draw();
}

// call-back function for button "Clear"
void clear() { 
	graph.reset(image,s,r);
	draw();
}

// call-back function for button "Reset"
void reset() { 
	graph.reset(image,s,r,false); // clears current segmentation but keeps seeds or box - function in graphcuts.cpp
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
void view_set(bool v) {view=v; draw();}  

// call-back function for setting parameter "s" 
// (noise sensitivity) 
void s_set(const char* s_string) {
	sscanf_s(s_string,"s=%f",&s);
	cout << "parameter 's' is set to " << s << endl;
}

// call-back function for setting parameter "r" 
// (data/color tern weight) 
void r_set(const char* r_string) {
	sscanf_s(r_string,"r=%f",&r);
	cout << "parameter 'r' is set to " << r << endl;
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
	if      (drag==1) for (i=start; i<end; ++i) graph.addSeed(click+(*i),OBJ); // function addSeed() is defined in graphV.h
	else if (drag==2) for (i=start; i<end; ++i) graph.addSeed(click+(*i),BKG);
	else if (drag==3) for (i=start; i<end; ++i) graph.addSeed(click+(*i),NONE);
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

// sets top_left (tl) and bottom_right (br) corners of the box for GrabCut 
void set_gc_box(Point tl, Point br)
{
	Point gc_box[2];
	if (tl.x <  br.x && tl.y <  br.y) {gc_box[0] = tl; gc_box[1] = br;}
	if (tl.x >= br.x && tl.y >= br.y) {gc_box[0] = br; gc_box[1] = tl;}
	if (tl.x <  br.x && tl.y >= br.y) {gc_box[0].x = tl.x; gc_box[0].y = br.y; gc_box[1].x = br.x; gc_box[1].y = tl.y;}
	if (tl.x >= br.x && tl.y <  br.y) {gc_box[0].x = br.x; gc_box[0].y = tl.y; gc_box[1].x = tl.x; gc_box[1].y = br.y;}
	graph.setBox(gc_box[0],gc_box[1]);
	SetDrawColour(126,126,126); DrawRectangleOutline(gc_box[0].x,gc_box[0].y,gc_box[1].x,gc_box[1].y);
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
	
	if (!image.isEmpty()) drawImage(image); // draws image (object defined in graphcuts.cpp) using global function in Image2D.h (1st draw method there)
	else {SetDrawColour(255, 0, 0); DrawText(2,2,"image was not found"); return;}

	// look-up tables specifying colors and transparancy for each possible integer value (FREE=0,OBJ=1,BKG=2) in "labeling"
	RGB    colors[3]        = { black,  red,  blue};
	double transparancy1[3] = { 0,      0.2,   0.2};
	double transparancy2[3] = { 0,      1.0,   1.0};
	if (!graph.getLabeling().isEmpty()) drawImage(graph.getLabeling(),colors,transparancy1); // 4th draw() function in Image2D.h
	if (!graph.getSeeds().isEmpty())    drawImage(graph.getSeeds(),colors,transparancy2); // 4th draw() function in Image2D.h
	if (view) {
		RGB    colorsF[]       = { black, green};
		double transparancyF[] = { 0.0,   0.4};
		drawImage(graph.getFlow(),colorsF,transparancyF);
	}
	if (graph.getBox().size()==2) {SetDrawColour(255,255,0); DrawRectangleOutline(graph.getBox()[0].x,graph.getBox()[0].y,graph.getBox()[1].x,graph.getBox()[1].y);}

}

// call-back function for button "SaveA"
void image_saveA() 
{
	// FEEL FREE TO MODIFY THE CODE BELOW TO SAVE WHATEVER IMAGE YOU WOULD LIKE!!!
	// THIS VERSION SAVES A VIEW SIMILAR TO WHAT IS DRAWN ON THE SCREEN (in function draw() above)
	cout << "saving the results into image file... ";
	Table2D<RGB> tmp;

	// SAVES REGION MASK OVER THE ORIGINAL IMAGE (similar to what is drawn on the window)
	RGB    colors[3]        = { black,  red,  blue};
	double transparancy1[3] = { 0,      0.2,   0.2};
	double transparancy2[3] = { 0,      1.0,   1.0};
	Table2D<RGB>    cMat1 = convert<RGB>(graph.getLabeling(),Palette(colors));   //3rd convert method in Table2D.h
	Table2D<double> aMat1 = convert<double>(graph.getLabeling(),Palette(transparancy1)); 
	tmp = cMat1%aMat1 + image%(1-aMat1);  // painting labeling over image
	Table2D<RGB>    cMat2 = convert<RGB>(graph.getSeeds(),Palette(colors));   //3rd convert method in Table2D.h
	Table2D<double> aMat2 = convert<double>(graph.getSeeds(),Palette(transparancy2)); 
	tmp = cMat2%aMat2 + tmp%(1-aMat2); // painting seeds
	if (view) {
		RGB    colorsF[]       = { black, green};
		double transparancyF[] = { 0.0,   0.4};
		Table2D<RGB>    cMat3 = convert<RGB>(graph.getFlow(),Palette(colorsF));   //3rd convert method in Table2D.h
		Table2D<double> aMat3 = convert<double>(graph.getFlow(),Palette(transparancyF)); 
		tmp = cMat3%aMat3 + tmp%(1-aMat3); // painting flow
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
	RGB    colors[3]        = { white,  black,  white};
	double transparancy[3]  = {  0.8,     0,    0.8  };
	Table2D<RGB>    cMat = convert<RGB>(graph.getLabeling(),Palette(colors));   //3rd convert method in Table2D.h
	Table2D<double> aMat = convert<double>(graph.getLabeling(),Palette(transparancy)); 
	tmp = cMat%aMat + image%(1-aMat);  // painting labeling over image
	
	save_counter[im_index]++;
	string name(to_Cstr("Results/result_" << image_names[im_index] << save_counter[im_index] << ".bmp"));
	saveImage(tmp, to_Cstr(name)); // saves to BMP file
	cout << name << endl; // prints the name of the saved .bmp file on the console
}
