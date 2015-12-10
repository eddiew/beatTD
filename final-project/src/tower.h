#pragma once

#include "ofxMidiOut.h"
#include <map>
using namespace std;

struct hit {
	float damage; // TODO: more interesting hit types

	hit(float damage) : damage(damage) {}
};

class tower
{
public:
	tower(ofxMidiOut& midiOut, ofPoint position, int pitch, ofColor color, float dpb_base = 1. / 4, float range = 1. / 4, int channel = 10, int program = 0)
		: midiOut(midiOut), midiChannel(channel), midiProgram(program), pitch(pitch), color(color), position(position), dpb_base(dpb_base), range(range), level(1) {}
	bool play(float tNote, float tNote_prev);
	void beginRecording(float tNote);
	float recordHit(float tNote); // returns hit damage
	float getHitDamage(float tNote); // returns hit damage of the hit directly before tNote
	void endRecording(float tNote);

	inline int getBars() { return bars; }
	inline bool isRecording() { return recording; }

	inline ofPoint& getPosition() { return position; }
	inline float getRange() { return range; }

	inline const ofColor& getColor() { return color; }

	void levelUp();
	int getUpgradeCost();

private:
	ofxMidiOut& midiOut;
	float *tempo;

	map<float, hit> hits;
	int midiChannel = 1;
	int midiProgram = 0;
	int pitch; // corresponds to instrument with drum kit

	int bars = 1;

	bool recording = false;
	float recordingStartTime = 0;

	void playNote();

	ofPoint position;
	float range; // in units of h

	ofColor color;

	float dpb_base;
	float getDpb(); // damage per beat
	int level;
};
