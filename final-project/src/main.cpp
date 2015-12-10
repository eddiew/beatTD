#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){
	// I have a 4k display. You might want
	// to lower these values for your own use.
	ofSetupOpenGL(3200,1800,OF_WINDOW);			// <-------- setup the GL context

	// this kicks off the running of my app
	// can be OF_WINDOW or OF_FULLSCREEN
	// pass in width and height too:
	ofRunApp(new ofApp());

}
