#include "ofApp.h"
#include <string>
#include <utility>

//--------------------------------------------------------------
void ofApp::setup(){
	ofBackground(255);

	// Open songs directory
	songFiles = ofDirectory("songs").getFiles();
	songIterator = songFiles.begin();

	// Connect to VirtualMidi
	midiOut.listPorts();
	midiOut.openPort(0);

	// Use channel 16 for metronome.
	// Metronome sound = woodblock
	midiOut.sendProgramChange(16, 115);
	
	// Use electric piano for enemies
	midiOut.sendProgramChange(15, 2);

	// Set drum channel (10) to electric drum kit
	midiOut.sendProgramChange(10, 24);

	// Initialize window size
	ofPoint windowSize = ofGetWindowSize();
	windowResized(windowSize.x, windowSize.y);

	// Initialize UI
	topTowerMatrix = new ofxDatGuiMatrix("", towers_per_group, false, new towerMatrixTemplate(w, h));
	topTowerMatrix->setOrigin(w0, wh / 2 - w / 16 - 6 * h / 64);
	topTowerMatrix->onMatrixEvent(this, &ofApp::onMatrixEvent);

	centerTowerMatrix = new ofxDatGuiMatrix("", towers_per_group, false, new towerMatrixTemplate(w, h));
	centerTowerMatrix->setOrigin(w0, wh / 2 + 7 * h / 64);
	centerTowerMatrix->onMatrixEvent(this, &ofApp::onMatrixEvent);

	// Main GUI panel
	gameGui = new ofxDatGui(w0, h0);
	gameGui->setTemplate(new gameGuiTemplate(w, topTowerMatrix->getY()));
	gameGui->setWidth(w / 2);
	gameGui->addFRM();
	metronomeToggle = gameGui->addToggle("Toggle Metronome", true);
	moneyText = gameGui->addTextInput("Credits", to_string(credits)); // TODO: prevent user from editing
	healthSlider = gameGui->addSlider("Base integrity", 0, 100, 100);

	// Wave GUI panel
	waveGui = new ofxDatGui(ww / 2, h0);
	waveGui->setTemplate(new gameGuiTemplate(w, topTowerMatrix->getY()));
	waveGui->setWidth(w / 2);
	waveNumberText = waveGui->addTextInput("Wave Number:", to_string(waveCounter));
	enemyHealthText = waveGui->addTextInput("Enemy Health:", to_string(enemyHealth));
	nextEnemyHealthText = waveGui->addTextInput("Next Enemy Health:", to_string(nextEnemyHealth));
	sendWaveButton = waveGui->addButton("Send next wave");
	waveGui->onButtonEvent(this, &ofApp::onWaveButtonEvent);

	// TODO: more details
	// Tower GUI Panel
	// stats, hits, upgrades
	int towerGuiY = centerTowerMatrix->getY() + centerTowerMatrix->getHeight();
	towerGui = new ofxDatGui(w0 + w/4, towerGuiY); // TODO: h
	towerGui->setTemplate(new bottomGuiTemplate(w, h - towerGuiY));
	towerGui->setWidth(w / 2);
	//towerGui->addHeader("Tower Detail")->setDraggable(false);
	buildRedTowerButton = towerGui->addButton("Build Red Tower (100)");
	buildGreenTowerButton = towerGui->addButton("Build Green Tower (100)");
	buildBlueTowerButton = towerGui->addButton("Build Blue Tower (100)");
	upgradeTowerButton = towerGui->addButton("Upgrade tower");
	towerGui->onButtonEvent(this, &ofApp::onTowerButtonEvent);
	// TODO: more details
	// Tower stats, hits, upgrades

	queueWave();

	ofSetCircleResolution(128);
}

float nextBar(float tNote) {
	return 4 * ceil(tNote / 4);
}

//--------------------------------------------------------------
void ofApp::update() {
	topTowerMatrix->update();
	centerTowerMatrix->update();
	float t = ofGetElapsedTimef();
	float deltaT = t - t_prev;
	float tNote = tNote_prev + deltaT / getNoteDur();

	// Unstepped processing
	// ----------------------------------
	
	// Spawn enemies
	if (sendingWave && noteIterator != notes.end() && tNote - waveOnset > noteIterator->tOnset) {
		note& n = *noteIterator;
		// play sound
		midiOut.sendNoteOn(15, n.pitch, 64);

		// spawn enemy
		enemy *e = new enemy(n.tOnset + waveOnset, n.pitch, enemyHealth, waveCounter);
		enemies.insert(e);

		noteIterator++;
		if (noteIterator == notes.end()) {
			sendingWave = false;
		}
	}

	// Play towers
	// TODO: shoot stuff.
	for (pair<int, tower> pair : towers) {
		tower& t = pair.second;
		bool hit = t.play(tNote, tNote_prev);
		if (!hit) continue;

		// TODO: different shoot types, improve search efficiency, visual indicator
		// Currently shoots all enemies in range.
		areaHit(t.getPosition(), t.getRange(), t.getHitDamage(tNote), tNote, t.getColor());
	}

	// Check if enemies have made it all the way
	auto enemyIt = enemies.begin();
	while (enemyIt != enemies.end()) {
		if ((*enemyIt)->getProgress(tNote) >= 1) {
			delete *enemyIt;
			enemyIt = enemies.erase(enemyIt);
			hitPlayer(0.05);
			continue;
		}
		enemyIt++;
	}

	// delete decals when they're too old
	auto decalIt = decals.begin();
	while (decalIt != decals.end()) {
		if ((*decalIt)->shouldDelete(tNote)) {
			delete *decalIt;
			decalIt = decals.erase(decalIt);
			continue;
		}
		decalIt++;
	}

	// Skip stepped logic if not at a step
	int step = floor(tNote);
	if (step == floor(tNote_prev))
		goto end;
	// Stepped processing
	// ----------------------------------

	// Metronome
	if (metronomeToggle->getEnabled()) {
		midiOut.sendNoteOn(16, 64, 64);
		if (step % 4 == 0)
			midiOut.sendNoteOn(16, 88, 128);
	}

	// End processing
	// ----------------------------------
	// Remember last update time
end:
	t_prev = t;
	tNote_prev = tNote;
}

//--------------------------------------------------------------
void ofApp::draw() {
	float tNote = getTNote();
	topTowerMatrix->draw();
	centerTowerMatrix->draw();

	// Draw staff. TODO: less boring staff
	float thickness = h / 384;
	ofSetLineWidth(thickness);
	ofSetColor(0);
	for (int i = 0; i < 5; i++) {
		int note = 2 * (i + 1);
		ofPoint leftVertex = getScreenPos(0, note);
		ofPoint rightVertex = getScreenPos(1, note);
		ofDrawLine(leftVertex, rightVertex);
	}

	// Draw selected tower indicator
	if (selectedSquare != -1) {
		ofPoint& pos = getSquareCenter(selectedSquare);

		tower *t = getSelectedTower();
		if (t) {
			// Draw selected tower range
			ofSetColor(t->getColor().r, t->getColor().g, t->getColor().b, t->getColor().a * (100. /255));
			ofNoFill();
			ofCircle(pos, t->getRange() * h);
			ofFill();
		}

		ofSetColor(ofColor::fromHsb(255 / 6, 255, 255, 100));
		float squareSize = topTowerMatrix->getHeight() / 2;
		ofDrawRectangle(pos.x - squareSize / 2, pos.y - squareSize / 2, squareSize, squareSize);
	}

	// Draw decals
	for (decal *d : decals)
		d->draw(tNote);

	// Draw enemies
	for (enemy *e : enemies)
		drawEnemy(*e, tNote);

	// Draw towers
	for (pair<int, tower> pair : towers) {
		tower& t = pair.second;
		drawTower(t, tNote);
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	// testing
	switch (key) {
	case ',':
		currentInstrument = fmod(currentInstrument - 1, 127);
		midiOut.sendNoteOn(10, currentInstrument, 127);
		cout << currentInstrument << endl;
		break;

	case '.':
		currentInstrument = fmod(currentInstrument + 1, 127);
		midiOut.sendNoteOn(10, currentInstrument, 127);
		cout << currentInstrument << endl;
		break;

	case '/':
		midiOut.sendNoteOn(10, currentInstrument, 127);
		cout << currentInstrument << endl;
		break;
	}

	tower *t = getSelectedTower();
	if (t == NULL)
		return;

	float tNote = getTNote();
	float damage;
	switch (key) {
	// Record while 'f' is pressed
	case 'f':
		if (!t->isRecording())
			t->beginRecording(tNote);
		break;

	// Record notes with spacebar
	case ' ':
		damage = t->recordHit(tNote);
		if (damage > 0)
			areaHit(t->getPosition(), t->getRange(), t->getHitDamage(tNote), tNote, t->getColor());
		break;
	}

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
	if (getSelectedTower() == NULL)
		return;

	float tNote = getTNote();
	switch (key) {
	case 'f':
		getSelectedTower()->endRecording(tNote);
		break;
	}
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
	this->ww = w;
	this->wh = h;
	this->h = min(ww / aspectRatio, (float) wh);
	this->w = min((float) ww, wh * aspectRatio);
	this->w0 = (ww - w) / 2;
	this->h0 = (wh - h) / 2;
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

float ofApp::getTNote() {
	float t = ofGetElapsedTimef();
	float deltaT = t - t_prev;
	return tNote_prev + deltaT / getNoteDur();
}

float ofApp::getYPos(int enemyY) {
	return wh / 2 - (h / 64) * (enemyY % 12 - 6);
}

// x is from 0 - 1,
// y is in notes. centered at y = 6
ofPoint ofApp::getScreenPos(float x, float y) {
	ofPoint position;
	position.x = w0 + w * x;
	position.y = getYPos(y);
	return position;
}

// TODO: less boring tracks
// currently just goes left to right.
ofPoint ofApp::getEnemyPosition(enemy& e, float tNote) {
	return getScreenPos(e.getProgress(tNote), e.y);
}

void ofApp::onTowerButtonEvent(ofxDatGuiButtonEvent e)
{
	if (e.target == buildRedTowerButton) {
		if (selectedSquare != -1 && getSelectedTower() == NULL &&
			credits >= 100) {
			subtractCredits(100);
			addTower(selectedSquare, ofColor::red, 35);
		}
	}
	else if (e.target == buildGreenTowerButton) {
		if (selectedSquare != -1 && getSelectedTower() == NULL &&
			credits >= 100) {
			subtractCredits(100);
			addTower(selectedSquare, ofColor::green, 39);
		}
	}
	else if (e.target == buildBlueTowerButton) {
		if (selectedSquare != -1 && getSelectedTower() == NULL &&
			credits >= 100) {
			subtractCredits(100);
			addTower(selectedSquare, ofColor::blue, 49);
		}
	}
	else if (e.target == upgradeTowerButton) {
		tower *t = getSelectedTower();
		if (t == NULL || credits < t->getUpgradeCost())
			return;
		subtractCredits(t->getUpgradeCost());
		t->levelUp();
	}
}

void ofApp::onWaveButtonEvent(ofxDatGuiButtonEvent e)
{
	if (e.target == sendWaveButton) {
		sendWave();
	}
}

void ofApp::onMatrixEvent(ofxDatGuiMatrixEvent e)
{
	int square = e.child + towers_per_group * (e.target == centerTowerMatrix);
	selectedSquare = square;
}

ofPoint ofApp::getSquareCenter(int square) {
	int button = square % towers_per_group;
	ofxDatGuiMatrix *matrix = square < towers_per_group ? topTowerMatrix : centerTowerMatrix;
	return matrix->getCenterOfChild(button);
}

// Instrument list
// 35 = 'boots'
// 39 = 'cats'
// 49 = cymbals
void ofApp::addTower(int square, const ofColor& color, int instrument) {
	tower t(midiOut, getSquareCenter(square), instrument, color);
	// instruments
	// 113 agogo
	// 114 steel drum
	// 115 woodblock (metronome sound)
	// 116 taiko
	// 117 melodic tom
	// 118 synth drum
	towers.insert(make_pair(square, t));
}

// TODO: animate
void ofApp::drawEnemy(enemy& e, float tNote) {
	// Draw enemy body
	// TODO: draw notes instead of circles?
	float sizeFactor = 1 + sin(tNote * PI) / 10;
	float enemySize = h / 96 * sizeFactor;
	ofPoint position = getEnemyPosition(e, tNote);
	ofSetColor(e.colorDamage / 4);
	ofCircle(position, enemySize);

	// draw health bar
	ofSetColor(ofColor::fromHsb(255. * e.getHealthiness() / 3, 255, 255));
	float healthBarY = position.y - h / 64;
	float healthBarX1 = position.x - w * e.getHealthiness() / 128;
	float healthBarX2 = position.x + w * e.getHealthiness() / 128;
	ofDrawRectRounded(healthBarX1, healthBarY, healthBarX2 - healthBarX1, h / 384, h / 768);
}

void ofApp::drawTower(tower& t, float tNote) {
	// TODO: replace with something less lame
	ofSetColor(t.getColor());
	float tf = 1 + sin(tNote * PI) / 16;
	ofCircle(t.getPosition(), tf * h / 64);
}

// Returns true if enemy died;
bool ofApp::hitEnemy(enemy *e, float damage, const ofColor& color) {
	float damageMultiplier = 1 + ((float)(color.r * e->colorDamage.g + color.g * e->colorDamage.b + color.b * e->colorDamage.r)) / 255;
	e->health -= damage * damageMultiplier;
	e->colorDamage += color * damage;
	return e->health <= 1e-5;
}

void ofApp::addCredits(int amount) {
	credits += amount;
	moneyText->setText(to_string(credits));
}

void ofApp::subtractCredits(int amount) {
	credits -= amount;
	moneyText->setText(to_string(credits));
}

tower* ofApp::getSelectedTower() {
	if (towers.find(selectedSquare) == towers.end())
		return NULL;
	return &towers.at(selectedSquare);
}

void ofApp::areaHit(ofPoint& center, float radius, float damage, float tNote, const ofColor& color) {
	// TODO: different shoot types, improve search efficiency, visual indicator
	// Currently shoots all enemies in range.
	float range = radius * h; // because towers don't know h
	auto enemyIt = enemies.begin();
	while (enemyIt != enemies.end()) {
		ofPoint enemyPos = getEnemyPosition(**enemyIt, tNote);
		if (center.squareDistance(enemyPos) < range * range) {
			bool dead = hitEnemy(*enemyIt, damage, color);
			if (dead) {
				addCredits((*enemyIt)->reward);
				delete *enemyIt;
				enemyIt = enemies.erase(enemyIt);
				continue;
			}
		}
		enemyIt++;
	}
	// add decal
	circleDecal *hitDecal = new circleDecal(tNote, 1, center, ofColor(color.r, color.g, color.g, color.a * (64./255)), radius * h, decal::FADE);
	circleDecal *hitDecal2 = new circleDecal(tNote, 0.5, center, ofColor(color.r, color.g, color.b, color.a * (128. / 255)), radius * h, decal::FADE | decal::GROW);
	circleDecal *hitDecal3= new circleDecal(tNote, 1, center, ofColor(color.r, color.g, color.b, color.a * (128. / 255)), radius * h / 2, decal::SHRINK | decal::FADEIN);
	decals.insert(hitDecal);
	decals.insert(hitDecal2);
	decals.insert(hitDecal3);
}

void ofApp::sendWave() {
	notes = nextNotes;
	enemyHealth = nextEnemyHealth;
	waveNumberText->setText(to_string(++waveCounter));
	enemyHealthText->setText(to_string(enemyHealth));
	noteIterator = notes.begin();
	queueWave();
	waveOnset = nextBar(getTNote());
	sendingWave = true;
}

void ofApp::queueWave() {
	// Load song file
	string& songFilePath = songIterator->path();
	ofxCsv csv;
	csv.loadFile(songFilePath, ", ");

	// Populate note list
	nextNotes.clear();
	nextNotes.reserve(csv.numRows / 2);
	for (int row = 0; row < csv.numRows; row++) {
		float tOnset = (float)csv.getInt(row, 1) / 96;
		float pitch = csv.getInt(row, 4);
		// TODO: note offs
		string command = csv.getString(row, 2);
		if (command == "Note_on_c") {
			note n(pitch, tOnset);
			nextNotes.push_back(n);
		}
	}

	// Set next enemy
	// TODO: more enemy variety
	nextEnemyHealth = pow(1.2, waveCounter);
	nextEnemyHealthText->setText(to_string(nextEnemyHealth));

	songIterator++;
	if (songIterator == songFiles.end())
		songIterator = songFiles.begin();
	// TODO: display current, next wave info on wave gui panel
}

void ofApp::hitPlayer(float damage) {
	baseHealth -= damage;
	healthSlider->setValue((int)(baseHealth * 100));
	// TODO: check for 0 health
}
