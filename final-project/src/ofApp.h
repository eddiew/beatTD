#pragma once
#include "ofMain.h"
#include "ofxCsv.h"
#include "ofxDatGui.h"
#include "ofxMidi.h"
#include "tower.h"
#include <map>
#include <set>
#include <vector>
using namespace std;
using namespace wng;

#define towers_per_row 32
#define towers_per_group (2 * towers_per_row)

struct note {
	note(int pitch, float tOnset) : pitch(pitch), tOnset(tOnset) {}
	int pitch;
	float tOnset;
	float duration; // TODO: use?
};

struct enemy {
	enemy(float tSpawn, int y, float health, float reward, float speed = 1. / 32)
		: tSpawn(tSpawn), y(y), health(health), maxHealth(health), reward(reward), speed(speed) {}
	float health;
	float maxHealth;
	float speed;
	float tSpawn;
	int reward;
	int y;
	ofColor colorDamage = ofColor::black;

	float getProgress(float tNote) {
		return (tNote - tSpawn) * speed;
	}

	inline float getHealthiness() { return health / maxHealth; }
};

class gameGuiTemplate : public ofxDatGuiTemplate {
public:
	gameGuiTemplate(int w, int h) {
		row.height = h / 4;
		row.padding = h / 64;
		font.size = row.height / 8;
		row.label.maxAreaWidth = row.height * 3;
		row.stripeWidth = row.padding;
		init();
	}
};

class bottomGuiTemplate : public gameGuiTemplate {
public:
	bottomGuiTemplate(int w, int h) : gameGuiTemplate(w, h) {
		row.height = h / 4;
		init();
	}
};

class towerMatrixTemplate : public ofxDatGui2880x1800
{
public:
	towerMatrixTemplate(int w, int h) {
		// Row settings
		row.padding = h / 384;
		row.stripeWidth = 0;
		row.width = w;
		// Matrix settings
		matrix.buttonSpacing = row.padding;
		matrix.buttonSize = (w - row.padding) / towers_per_row - row.padding;
		// Hide label
		row.label.maxAreaWidth = row.padding;
		// Colors
		row.color.bkgd = ofColor::fromHex(0);
		matrix.color.normal.button = ofColor::fromHex(0xffffff);
		matrix.color.selected.button = matrix.color.normal.button; // Hide button selection

		init(); // required
	}
};

class decal {
public:
	float tSpawn;
	float ttl;
	ofPoint pos;
	ofColor color;
	int effectsMask;

	static const int FADE = 1;
	static const int FADEIN = 2;
	static const int GROW = 4;
	static const int SHRINK = 8;

	virtual void draw_(float age, float sizeFactor) = 0; // age : [0,1) as proportion of lifespan

	decal(float tSpawn, float ttl, ofPoint& pos, ofColor& color, int effectsMask = 0)
		: tSpawn(tSpawn), ttl(ttl), pos(pos), color(color), effectsMask(effectsMask) {}

	void draw(float tNote) {
		float age = (tNote - tSpawn) / ttl;

		float alpha = 1;
		if (effectsMask & FADE)
			alpha = 1 - age;
		else if (effectsMask & FADEIN)
			alpha = age;

		float sizeFactor = 1;
		if (effectsMask & GROW)
			sizeFactor = age;
		else if (effectsMask & SHRINK)
			sizeFactor = 1 - age;

		ofColor c(color.r, color.g, color.b, color.a * alpha);
		ofSetColor(c);
		draw_(age, sizeFactor);
	}

	bool shouldDelete(float tNote) {
		return tNote > tSpawn + ttl;
	}
};

class circleDecal : public decal {
public:
	circleDecal(float tSpawn, float ttl, ofPoint& pos, ofColor& color, float radius, int effectsMask = 0)
		: radius(radius), decal(tSpawn, ttl, pos, color, effectsMask) {}
	float radius;
	void draw_(float age, float sizeFactor) {
		float rad = radius * sizeFactor;
		ofCircle(pos, rad);
	}
};

class ofApp : public ofBaseApp{

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);
		
	// Time signature
	int beatsPerBar = 4;
	float tempo = 120; // beats per minute
	inline float getNoteDur() { return 60. / tempo; } // seconds per note
	inline float getBarDur() { return getNoteDur() * 4; } // seconds per bar

private:
	ofxMidiOut midiOut;

	ofxDatGuiMatrix* topTowerMatrix;
	ofxDatGuiMatrix* centerTowerMatrix;
	void onMatrixEvent(ofxDatGuiMatrixEvent e);

	ofxDatGui* gameGui;
	ofxDatGuiToggle* metronomeToggle;
	ofxDatGuiTextInput* moneyText;
	ofxDatGuiSlider* healthSlider;

	ofxDatGui* waveGui;
	ofxDatGuiTextInput* waveNumberText;
	ofxDatGuiTextInput* enemyHealthText;
	ofxDatGuiTextInput* nextEnemyHealthText;
	ofxDatGuiButton* sendWaveButton;
	void onWaveButtonEvent(ofxDatGuiButtonEvent e);

	ofxDatGui* towerGui;
	ofxDatGuiButton* buildRedTowerButton;
	ofxDatGuiButton* buildGreenTowerButton;
	ofxDatGuiButton* buildBlueTowerButton;
	ofxDatGuiButton* upgradeTowerButton;
	void onTowerButtonEvent(ofxDatGuiButtonEvent e);
	
	// last update time
	float t_prev = 0;
	float tNote_prev = 0;
	float waveOnset = 0;
	bool sendingWave = false;

	float getTNote();

	int selectedSquare = -1;
	tower* getSelectedTower();
	ofPoint getSquareCenter(int square);
	void addTower(int square, const ofColor& color, int instrument);

	// Towers
	map<int, tower> towers;

	float getYPos(int enemyY);
	ofPoint getScreenPos(float x, float y);
	ofPoint getEnemyPosition(enemy& e, float tNote);

	void drawEnemy(enemy& e, float tNote);
	void drawTower(tower& t, float tNote);

	// Decals
	set<decal*> decals;

	// Dimensions
	float ww, wh; // Window size
	float w, h; // Display region size
	float w0, h0; // Top left corner of display region
	float aspectRatio = 16. / 9;

	// Must be sorted by increasing tOnse
	vector<note> notes;
	vector<note> nextNotes;
	vector<note>::iterator noteIterator;

	float enemyHealth = 0;
	float nextEnemyHealth = 1;
	set<enemy*> enemies;

	void areaHit(ofPoint& center, float radius, float damage, float tNote, const ofColor& color);
	// Returns true if enemy died.
	bool hitEnemy(enemy *e, float damage, const ofColor& color);

	// Money
	int credits = 100;
	void addCredits(int amount);
	void subtractCredits(int amount);

	vector<ofFile> songFiles;
	vector<ofFile>::iterator songIterator;
	void queueWave();
	void sendWave();
	int waveCounter = 0;

	float baseHealth = 1;

	void hitPlayer(float damage);

	// test
	int currentInstrument = 0;
};
