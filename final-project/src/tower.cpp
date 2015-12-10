#include "tower.h"
#include <utility>

bool tower::play(float tNote, float tNote_prev) {
	if (recording || hits.empty()) return false;
	float tNote_prev_end = fmod(tNote_prev, 4*bars);
	float tNote_end = tNote_prev_end + tNote - tNote_prev;
	auto curHit_end = hits.lower_bound(tNote_end);
	auto prevHit_end = hits.lower_bound(tNote_prev_end);

	float tNote_begin = fmod(tNote, 4 * bars);
	float tNote_prev_begin = tNote_begin - tNote + tNote_prev;
	auto curHit_begin = hits.lower_bound(tNote_begin);
	auto prevHit_begin = hits.lower_bound(tNote_prev_begin);

	if (curHit_end != prevHit_end || curHit_begin != curHit_begin) { // helps with weird edge case when note is on measure border
		playNote();
		return true;
	}
	return false;
}

void tower::beginRecording(float tNote) {
	recording = true;
	hits.clear();
	recordingStartTime = 4 * floor(tNote / 4);
	bars = 1;
}

float tower::recordHit(float tNote) {
	if (!recording)
		return -1;
	float t = tNote - recordingStartTime;
	if (t < 0)
		return -1;
	playNote();

	float prev_t;
	if (hits.empty())
		prev_t = 0;
	else
		prev_t = hits.rbegin()->first;
	float damage = getDpb() * (t - prev_t);
	hits.insert(make_pair(t, damage));
	return damage;
}

void tower::endRecording(float tNote) {
	recording = false;
	if (hits.empty())
		return;
	float bars = 1 + floor((hits.rbegin()->first - hits.begin()->first) / 4);

	// Adjust first hit damage
	float lastHitTime = hits.rbegin()->first;
	if (lastHitTime > bars * 4)
		hits.begin()->second.damage = (hits.begin()->first - fmod(lastHitTime, bars * 4)) * getDpb();

	// rotate hits to make everything line up on measure breaks
	auto rotate_start = hits.lower_bound(bars * 4);
	while (rotate_start != hits.end()) {
		float newTOnset = rotate_start->first - bars * 4;
		hit h = rotate_start->second;
		rotate_start = hits.erase(rotate_start);
		hits.insert(make_pair(newTOnset, h));
	}
}

float tower::getHitDamage(float tNote) {
	float tNote_mod = fmod(tNote, 4 * bars);
	auto hit = hits.lower_bound(tNote_mod);
	if (hit == hits.begin()) // necessary bc lower bound may wrap from end to begin
		return 0;
	return (--hit)->second.damage;
}

void tower::playNote() {
	midiOut.sendNoteOn(midiChannel, pitch, 64);
}

float tower::getDpb() {
	return dpb_base * pow(1.2, level);
}

void tower::levelUp() {
	level++;
}

int tower::getUpgradeCost() {
	return 50 * level;
}
