/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <.> hosxe < a t > gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <math.h>
#include "Timbre.h"
#include "Voice.h"

//#define DEBUG_ARP_STEP
enum ArpeggiatorDirection {
    ARPEGGIO_DIRECTION_UP = 0,
    ARPEGGIO_DIRECTION_DOWN,
    ARPEGGIO_DIRECTION_UP_DOWN,
    ARPEGGIO_DIRECTION_PLAYED,
    ARPEGGIO_DIRECTION_RANDOM,
    ARPEGGIO_DIRECTION_CHORD,
   /*
    * ROTATE modes rotate the first note played, e.g. UP: C-E-G -> E-G-C -> G-C-E -> repeat
    */
    ARPEGGIO_DIRECTION_ROTATE_UP, ARPEGGIO_DIRECTION_ROTATE_DOWN, ARPEGGIO_DIRECTION_ROTATE_UP_DOWN,
   /*
    * SHIFT modes rotate and extend with transpose, e.g. UP: C-E-G -> E-G-C1 -> G-C1-E1 -> repeat
    */
    ARPEGGIO_DIRECTION_SHIFT_UP, ARPEGGIO_DIRECTION_SHIFT_DOWN, ARPEGGIO_DIRECTION_SHIFT_UP_DOWN,

    ARPEGGIO_DIRECTION_COUNT
};

// TODO Maybe add something like struct ArpDirectionParams { dir, can_change, use_start_step }

inline static int __getDirection( int _direction ) {
	switch( _direction ) {
	case ARPEGGIO_DIRECTION_DOWN:
	case ARPEGGIO_DIRECTION_ROTATE_DOWN:
	case ARPEGGIO_DIRECTION_SHIFT_DOWN:
		return -1;
	default:
		return 1;
	}
}

inline static int __canChangeDir( int _direction ) {
	switch( _direction ) {
	case ARPEGGIO_DIRECTION_UP_DOWN:
	case ARPEGGIO_DIRECTION_ROTATE_UP_DOWN:
	case ARPEGGIO_DIRECTION_SHIFT_UP_DOWN:
		return 1;
	default:
		return 0;
	}
}

inline static int __canTranspose( int _direction ) {
	switch( _direction ) {
	case ARPEGGIO_DIRECTION_SHIFT_UP:
	case ARPEGGIO_DIRECTION_SHIFT_DOWN:
	case ARPEGGIO_DIRECTION_SHIFT_UP_DOWN:
		return 1;
	default:
		return 0;
	}
}

enum NewNoteType {
	NEW_NOTE_FREE = 0,
	NEW_NOTE_RELEASE,
	NEW_NOTE_OLD,
	NEW_NOTE_NONE
};


arp_pattern_t lut_res_arpeggiator_patterns[ ARPEGGIATOR_PRESET_PATTERN_COUNT ]  = {
  ARP_PATTERN(21845), ARP_PATTERN(62965), ARP_PATTERN(46517), ARP_PATTERN(54741),
  ARP_PATTERN(43861), ARP_PATTERN(22869), ARP_PATTERN(38293), ARP_PATTERN(2313),
  ARP_PATTERN(37449), ARP_PATTERN(21065), ARP_PATTERN(18761), ARP_PATTERN(54553),
  ARP_PATTERN(27499), ARP_PATTERN(23387), ARP_PATTERN(30583), ARP_PATTERN(28087),
  ARP_PATTERN(22359), ARP_PATTERN(28527), ARP_PATTERN(30431), ARP_PATTERN(43281),
  ARP_PATTERN(28609), ARP_PATTERN(53505)
};

uint16_t Timbre::getArpeggiatorPattern() const
{
  const int pattern = (int)params.engineArp2.pattern;
  if ( pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT )
    return ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[ pattern ]);
  else
    return ARP_PATTERN_GETMASK( params.engineArpUserPatterns.patterns[ pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT ] );
}

const uint8_t midi_clock_tick_per_step[17]  = {
  192, 144, 96, 72, 64, 48, 36, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1
};

extern float noise[32];


float panTable[] = {
		0.0000, 0.0007, 0.0020, 0.0036, 0.0055, 0.0077, 0.0101, 0.0128, 0.0156, 0.0186,
		0.0218, 0.0252, 0.0287, 0.0324, 0.0362, 0.0401, 0.0442, 0.0484, 0.0527, 0.0572,
		0.0618, 0.0665, 0.0713, 0.0762, 0.0812, 0.0863, 0.0915, 0.0969, 0.1023, 0.1078,
		0.1135, 0.1192, 0.1250, 0.1309, 0.1369, 0.1430, 0.1492, 0.1554, 0.1618, 0.1682,
		0.1747, 0.1813, 0.1880, 0.1947, 0.2015, 0.2085, 0.2154, 0.2225, 0.2296, 0.2369,
		0.2441, 0.2515, 0.2589, 0.2664, 0.2740, 0.2817, 0.2894, 0.2972, 0.3050, 0.3129,
		0.3209, 0.3290, 0.3371, 0.3453, 0.3536, 0.3619, 0.3703, 0.3787, 0.3872, 0.3958,
		0.4044, 0.4131, 0.4219, 0.4307, 0.4396, 0.4485, 0.4575, 0.4666, 0.4757, 0.4849,
		0.4941, 0.5034, 0.5128, 0.5222, 0.5316, 0.5411, 0.5507, 0.5604, 0.5700, 0.5798,
		0.5896, 0.5994, 0.6093, 0.6193, 0.6293, 0.6394, 0.6495, 0.6597, 0.6699, 0.6802,
		0.6905, 0.7009, 0.7114, 0.7218, 0.7324, 0.7430, 0.7536, 0.7643, 0.7750, 0.7858,
		0.7967, 0.8076, 0.8185, 0.8295, 0.8405, 0.8516, 0.8627, 0.8739, 0.8851, 0.8964,
		0.9077, 0.9191, 0.9305, 0.9420, 0.9535, 0.9651, 0.9767, 0.9883, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000
} ;



// Static to all 4 timbres
unsigned int voiceIndex  __attribute__ ((section(".ccmnoload")));


Timbre::Timbre() {


    this->recomputeNext = true;
    this->currentGate = 0;
    this->sbMax = &this->sampleBlock[64];
    this->holdPedal = false;

    // arpegiator
    setNewBPMValue(90);
    arpegiatorStep = 0.0;
    idle_ticks_ = 96;
    running_ = 0;
    ignore_note_off_messages_ = 0;
    recording_ = 0;
    note_stack.Init();
    event_scheduler.Init();
    // Arpeggiator start
    Start();


    // Init FX variables
    v0L = v1L = v0R = v1R = 0.0f;
    fxParam1PlusMatrix = -1.0;
}

Timbre::~Timbre() {
}

void Timbre::init(int timbreNumber) {
    struct LfoParams* lfoParams[] = { &params.lfoOsc1, &params.lfoOsc2, &params.lfoOsc3};
    struct StepSequencerParams* stepseqparams[] = { &params.lfoSeq1, &params.lfoSeq2};
    struct StepSequencerSteps* stepseqs[] = { &params.lfoSteps1, &params.lfoSteps2};

    matrix.init(&params.matrixRowState1);

	env1.init(&matrix, &params.env1a,  &params.env1b, ENV1_ATTACK);
	env2.init(&matrix, &params.env2a,  &params.env2b, ENV2_ATTACK);
	env3.init(&matrix, &params.env3a,  &params.env3b, ENV3_ATTACK);
	env4.init(&matrix, &params.env4a,  &params.env4b, ENV4_ATTACK);
	env5.init(&matrix, &params.env5a,  &params.env5b, ENV5_ATTACK);
	env6.init(&matrix, &params.env6a,  &params.env6b, ENV6_ATTACK);

	osc1.init(&matrix, &params.osc1, OSC1_FREQ);
	osc2.init(&matrix, &params.osc2, OSC2_FREQ);
	osc3.init(&matrix, &params.osc3, OSC3_FREQ);
	osc4.init(&matrix, &params.osc4, OSC4_FREQ);
	osc5.init(&matrix, &params.osc5, OSC5_FREQ);
	osc6.init(&matrix, &params.osc6, OSC6_FREQ);

    // OSC
    for (int k = 0; k < NUMBER_OF_LFO_OSC; k++) {
        lfoOsc[k].init(lfoParams[k], &this->matrix, (SourceEnum)(MATRIX_SOURCE_LFO1 + k), (DestinationEnum)(LFO1_FREQ + k));
    }

    // ENV
    lfoEnv[0].init(&params.lfoEnv1 , &this->matrix, MATRIX_SOURCE_LFOENV1, (DestinationEnum)0);
    lfoEnv2[0].init(&params.lfoEnv2 , &this->matrix, MATRIX_SOURCE_LFOENV2, (DestinationEnum)LFOENV2_SILENCE);

    // Step sequencer
    for (int k = 0; k< NUMBER_OF_LFO_STEP; k++) {
        lfoStepSeq[k].init(stepseqparams[k], stepseqs[k], &matrix, (SourceEnum)(MATRIX_SOURCE_LFOSEQ1+k), (DestinationEnum)(LFOSEQ1_GATE+k));
    }

	this->timbreNumber = timbreNumber;
}

void Timbre::setVoiceNumber(int v, int n) {
	voiceNumber[v] = n;
	if (n >=0) {
		voices[n]->setCurrentTimbre(this);
	}
}


void Timbre::initVoicePointer(int n, Voice* voice) {
	voices[n] = voice;
}

void Timbre::noteOn(char note, char velocity) {
	if (params.engineArp1.clock) {
		arpeggiatorNoteOn(note, velocity);
	} else {
		preenNoteOn(note, velocity);
	}
}

void Timbre::noteOff(char note) {
	if (params.engineArp1.clock) {
		arpeggiatorNoteOff(note);
	} else {
		preenNoteOff(note);
	}
}

int cptHighNote = 0;

void Timbre::preenNoteOn(char note, char velocity) {
	if (unlikely(note < 10)) {
		return;
	}

	int iNov = (int) params.engine1.numberOfVoice;
	if (unlikely(iNov == 0)) {
		return;
	}

	unsigned int indexMin = (unsigned int)2147483647;
	int voiceToUse = -1;

	int newNoteType = NEW_NOTE_NONE;

	for (int k = 0; k < iNov; k++) {
		// voice number k of timbre
		int n = voiceNumber[k];

#ifdef DEBUG_VOICE
    	if (unlikely(n<0)) {
    		lcd.setRealTimeAction(true);
    		lcd.clear();
    		lcd.setCursor(0,0);
    		lcd.print("Timbre::NoteOn");
			lcd.setCursor(0,1);
			lcd.print("nov: ");
			lcd.print((int)params.engine1.numberOfVoice);
			lcd.setCursor(10,1);
			lcd.print("k: ");
			lcd.print(k);
			lcd.setCursor(0,2);
			lcd.print("n: ");
			lcd.print(n);
			lcd.setCursor(10,2);
			lcd.print("t: ");
			lcd.print(timbreNumber);
			lcd.setCursor(0,3);
			lcd.print("note: ");
			lcd.print((int)note);
//			while (1);
    	}
#endif

		// same note = priority 1 : take the voice immediatly
		if (unlikely(voices[n]->isPlaying() && voices[n]->getNote() == note)) {
#ifdef DEBUG_VOICE
		lcd.setRealTimeAction(true);
		lcd.setCursor(16,1);
		lcd.print(cptHighNote++);
		lcd.setCursor(16,2);
		lcd.print("S:");
		lcd.print(n);
#endif
			voices[n]->noteOnWithoutPop(note, velocity, voiceIndex++);
			return;
		}

		// unlikely because if it true, CPU is not full
		if (unlikely(newNoteType > NEW_NOTE_FREE)) {
			if (!voices[n]->isPlaying()) {
				voiceToUse = n;
				newNoteType = NEW_NOTE_FREE;
			}

			if (voices[n]->isReleased()) {
				int indexVoice = voices[n]->getIndex();
				if (indexVoice < indexMin) {
					indexMin = indexVoice;
					voiceToUse = n;
					newNoteType = NEW_NOTE_RELEASE;
				}
			}
		}
	}

	if (voiceToUse == -1) {
		newNoteType = NEW_NOTE_OLD;
		for (int k = 0; k < iNov; k++) {
			// voice number k of timbre
			int n = voiceNumber[k];
			int indexVoice = voices[n]->getIndex();
			if (indexVoice < indexMin && !voices[n]->isNewNotePending()) {
				indexMin = indexVoice;
				voiceToUse = n;
			}
		}
	}
	// All voices in newnotepending state ?
	if (voiceToUse != -1) {
#ifdef DEBUG_VOICE
		lcd.setRealTimeAction(true);
		lcd.setCursor(16,1);
		lcd.print(cptHighNote++);
		lcd.setCursor(16,2);
		switch (newNoteType) {
			case NEW_NOTE_FREE:
				lcd.print("F:");
				break;
			case NEW_NOTE_OLD:
				lcd.print("O:");
				break;
			case NEW_NOTE_RELEASE:
				lcd.print("R:");
				break;
		}
		lcd.print(voiceToUse);
#endif
		switch (newNoteType) {
		case NEW_NOTE_FREE:
			voices[voiceToUse]->noteOn(note, velocity, voiceIndex++);
			break;
		case NEW_NOTE_OLD:
		case NEW_NOTE_RELEASE:
			voices[voiceToUse]->noteOnWithoutPop(note, velocity, voiceIndex++);
			break;
		}

	}
}


void Timbre::preenNoteOff(char note) {
	int iNov = (int) params.engine1.numberOfVoice;
	for (int k = 0; k < iNov; k++) {
		// voice number k of timbre
		int n = voiceNumber[k];

		// Not playing = free CPU
		if (unlikely(!voices[n]->isPlaying())) {
			continue;
		}

		if (likely(voices[n]->getNextGlidingNote() == 0)) {
			if (voices[n]->getNote() == note) {
				if (unlikely(holdPedal)) {
					voices[n]->setHoldedByPedal(true);
					return;
				} else {
					voices[n]->noteOff();
					return;
				}
			}
		} else {
			// if gliding and releasing first note
			if (voices[n]->getNote() == note) {
				voices[n]->glideFirstNoteOff();
				return;
			}
			// if gliding and releasing next note
			if (voices[n]->getNextGlidingNote() == note) {
				voices[n]->glideToNote(voices[n]->getNote());
				voices[n]->glideFirstNoteOff();
				return;
			}
		}
	}
}


void Timbre::setHoldPedal(int value) {
	if (value <64) {
		holdPedal = false;
	    int numberOfVoices = params.engine1.numberOfVoice;
	    for (int k = 0; k < numberOfVoices; k++) {
	        // voice number k of timbre
	        int n = voiceNumber[k];
	        if (voices[n]->isHoldedByPedal()) {
	        	voices[n]->noteOff();
	        }
	    }
	    arpeggiatorSetHoldPedal(0);
	} else {
		holdPedal = true;
	    arpeggiatorSetHoldPedal(127);
	}
}




void Timbre::setNewBPMValue(float bpm) {
	ticksPerSecond = bpm * 24.0f / 60.0f;
	ticksEveryNCalls = calledPerSecond / ticksPerSecond;
	ticksEveyNCallsInteger = (int)ticksEveryNCalls;
}

void Timbre::setArpeggiatorClock(float clockValue) {
	if (clockValue == CLOCK_OFF) {
		FlushQueue();
		note_stack.Clear();
	}
	if (clockValue == CLOCK_INTERNAL) {
	    setNewBPMValue(params.engineArp1.BPM);
	}
	if (clockValue == CLOCK_EXTERNAL) {
		// Let's consider we're running
		running_ = 1;
	}
}


void Timbre::prepareForNextBlock() {

	// Apeggiator clock : internal
	if (params.engineArp1.clock == CLOCK_INTERNAL) {
		arpegiatorStep+=1.0f;
		if (unlikely((arpegiatorStep) > ticksEveryNCalls)) {
			arpegiatorStep -= ticksEveyNCallsInteger;

			Tick();
		}
	}

	this->lfoOsc[0].nextValueInMatrix();
	this->lfoOsc[1].nextValueInMatrix();
	this->lfoOsc[2].nextValueInMatrix();
	this->lfoEnv[0].nextValueInMatrix();
	this->lfoEnv2[0].nextValueInMatrix();
	this->lfoStepSeq[0].nextValueInMatrix();
	this->lfoStepSeq[1].nextValueInMatrix();

    this->matrix.computeAllFutureDestintationAndSwitch();

    updateAllModulationIndexes();
    updateAllMixOscsAndPans();

}

void Timbre::cleanNextBlock() {
	float *sp = this->sampleBlock;
	while (sp < this->sbMax) {
		*sp++ = 0;
		*sp++ = 0;
		*sp++ = 0;
		*sp++ = 0;
		*sp++ = 0;
		*sp++ = 0;
		*sp++ = 0;
		*sp++ = 0;
	}
}



#define GATE_INC 0.02f

void Timbre::fxAfterBlock(float ratioTimbres) {
    // Gate algo !!
    float gate = this->matrix.getDestination(MAIN_GATE);
    if (unlikely(gate > 0 || currentGate > 0)) {
		gate *=.72547132656922730694f; // 0 < gate < 1.0
		if (gate > 1.0f) {
			gate = 1.0f;
		}
		float incGate = (gate - currentGate) * .03125f; // ( *.03125f = / 32)
		// limit the speed.
		if (incGate > 0.002f) {
			incGate = 0.002f;
		} else if (incGate < -0.002f) {
			incGate = -0.002f;
		}

		float *sp = this->sampleBlock;
		float coef;
    	for (int k=0 ; k< BLOCK_SIZE ; k++) {
			currentGate += incGate;
			coef = 1.0f - currentGate;
			*sp = *sp * coef;
			sp++;
			*sp = *sp * coef;
			sp++;
		}
    //    currentGate = gate;
    }

    // LP Algo
    int effectType = params.effect.type;
    float gainTmp =  params.effect.param3 * numberOfVoiceInverse * ratioTimbres;
    mixerGain = 0.02f * gainTmp + .98  * mixerGain;

    switch (effectType) {
    case FILTER_LP:
    {
    	float fxParamTmp = params.effect.param1 + matrix.getDestination(FILTER_FREQUENCY);
    	fxParamTmp *= fxParamTmp;

    	// Low pass... on the Frequency
    	fxParam1 = (fxParamTmp + 9.0f * fxParam1) * .1f;
    	if (unlikely(fxParam1 > 1.0f)) {
    		fxParam1 = 1.0f;
    	}
    	if (unlikely(fxParam1 < 0.0f)) {
    		fxParam1 = 0.0f;
    	}

    	float pattern = (1 - fxParam2 * fxParam1);

    	float *sp = this->sampleBlock;
    	float localv0L = v0L;
    	float localv1L = v1L;
    	float localv0R = v0R;
    	float localv1R = v1R;

    	for (int k=0 ; k < BLOCK_SIZE  ; k++) {

    		// Left voice
    		localv0L =  pattern * localv0L  -  (fxParam1) * localv1L  + (fxParam1)* (*sp);
    		localv1L =  pattern * localv1L  +  (fxParam1) * localv0L;

    		*sp = localv1L * mixerGain;

    		if (unlikely(*sp > ratioTimbres)) {
    			*sp = ratioTimbres;
    		}
    		if (unlikely(*sp < -ratioTimbres)) {
    			*sp = -ratioTimbres;
    		}

    		sp++;

    		// Right voice
    		localv0R =  pattern * localv0R  -  (fxParam1)*localv1R  + (fxParam1)* (*sp);
    		localv1R =  pattern * localv1R  +  (fxParam1)*localv0R;

    		*sp = localv1R * mixerGain;

    		if (unlikely(*sp > ratioTimbres)) {
    			*sp = ratioTimbres;
    		}
    		if (unlikely(*sp < -ratioTimbres)) {
    			*sp = -ratioTimbres;
    		}

    		sp++;
    	}
    	v0L = localv0L;
    	v1L = localv1L;
    	v0R = localv0R;
    	v1R = localv1R;
    }
    break;
    case FILTER_HP:
    {
    	float fxParamTmp = params.effect.param1 + matrix.getDestination(FILTER_FREQUENCY);
    	fxParamTmp *= fxParamTmp;

    	// Low pass... on the Frequency
    	fxParam1 = (fxParamTmp + 9.0f * fxParam1) * .1f;
    	if (unlikely(fxParam1 > 1.0)) {
    		fxParam1 = 1.0;
    	}
    	float pattern = (1 - fxParam2 * fxParam1);

    	float *sp = this->sampleBlock;
    	float localv0L = v0L;
    	float localv1L = v1L;
    	float localv0R = v0R;
    	float localv1R = v1R;

    	for (int k=0 ; k < BLOCK_SIZE ; k++) {

    		// Left voice
    		localv0L =  pattern * localv0L  -  (fxParam1) * localv1L  + (fxParam1) * (*sp);
    		localv1L =  pattern * localv1L  +  (fxParam1) * localv0L;

    		*sp = (*sp - localv1L) * mixerGain;

    		if (unlikely(*sp > ratioTimbres)) {
    			*sp = ratioTimbres;
    		}
    		if (unlikely(*sp < -ratioTimbres)) {
    			*sp = -ratioTimbres;
    		}

    		sp++;

    		// Right voice
    		localv0R =  pattern * localv0R  -  (fxParam1) * localv1R  + (fxParam1) * (*sp);
    		localv1R =  pattern * localv1R  +  (fxParam1) * localv0R;

    		*sp = (*sp - localv1R) * mixerGain;

    		if (unlikely(*sp > ratioTimbres)) {
    			*sp = ratioTimbres;
    		}
    		if (unlikely(*sp < -ratioTimbres)) {
    			*sp = -ratioTimbres;
    		}

    		sp++;
    	}
    	v0L = localv0L;
    	v1L = localv1L;
    	v0R = localv0R;
    	v1R = localv1R;

    }
	break;
    case FILTER_BASS:
    {
    	// From musicdsp.com
    	//    	Bass Booster
    	//
    	//    	Type : LP and SUM
    	//    	References : Posted by Johny Dupej
    	//
    	//    	Notes :
    	//    	This function adds a low-passed signal to the original signal. The low-pass has a quite wide response.
    	//
    	//    	selectivity - frequency response of the LP (higher value gives a steeper one) [70.0 to 140.0 sounds good]
    	//    	ratio - how much of the filtered signal is mixed to the original
    	//    	gain2 - adjusts the final volume to handle cut-offs (might be good to set dynamically)

    	//static float selectivity, gain1, gain2, ratio, cap;
    	//gain1 = 1.0/(selectivity + 1.0);
    	//
    	//cap= (sample + cap*selectivity )*gain1;
    	//sample = saturate((sample + cap*ratio)*gain2);

    	float *sp = this->sampleBlock;
    	float localv0L = v0L;
    	float localv0R = v0R;

    	for (int k=0 ; k < BLOCK_SIZE ; k++) {

    		localv0L = ((*sp) + localv0L * fxParam1) * fxParam3;
    		(*sp) = ((*sp) + localv0L * fxParam2) * mixerGain;

    		if (unlikely(*sp > ratioTimbres)) {
    			*sp = ratioTimbres;
    		}
    		if (unlikely(*sp < -ratioTimbres)) {
    			*sp = -ratioTimbres;
    		}

    		sp++;

    		localv0R = ((*sp) + localv0R * fxParam1) * fxParam3;
    		(*sp) = ((*sp) + localv0R * fxParam2) * mixerGain;

    		if (unlikely(*sp > ratioTimbres)) {
    			*sp = ratioTimbres;
    		}
    		if (unlikely(*sp < -ratioTimbres)) {
    			*sp = -ratioTimbres;
    		}

    		sp++;
    	}
    	v0L = localv0L;
    	v0R = localv0R;

    }
    break;
    case FILTER_MIXER:
    {
    	float pan = params.effect.param1 * 2 - 1.0f ;
    	float *sp = this->sampleBlock;
    	float sampleR, sampleL;
    	if (pan <= 0) {
        	float onePlusPan = 1 + pan;
        	float minusPan = - pan;
        	for (int k=0 ; k < BLOCK_SIZE  ; k++) {
				sampleL = *(sp);
				sampleR = *(sp + 1);

				*sp = (sampleL + sampleR * minusPan) * mixerGain;
				sp++;
				*sp = sampleR * onePlusPan * mixerGain;
				sp++;
			}
    	} else if (pan > 0) {
        	float oneMinusPan = 1 - pan;
        	float adjustedmixerGain = (pan * .5) * mixerGain;
        	for (int k=0 ; k < BLOCK_SIZE ; k++) {
				sampleL = *(sp);
				sampleR = *(sp + 1);

				*sp = sampleL * oneMinusPan * mixerGain;
				sp++;
				*sp = (sampleR + sampleL * pan) * mixerGain;
				sp++;
			}
    	}
    }
    break;
    case FILTER_CRUSHER:
    {
        // Algo from http://www.musicdsp.org/archive.php?classid=4#139
        // Lo-Fi Crusher
        // Type : Quantizer / Decimator with smooth control
        // References : Posted by David Lowenfels


        float fxFreq = params.effect.param1 ;

        float *sp = this->sampleBlock;
        float sampleR, sampleL;

        float localv0L = v0L;
        float localv0R = v0R;
        float localPhase = fxPhase;
        float localPower = fxParam1;
        float localStep = fxParam2;

        for (int k=0 ; k < BLOCK_SIZE ; k++) {
            localPhase += fxFreq;
            if (unlikely(localPhase > 1.0f)) {
                localPhase -= 1.0f;
                // Simulate floor by making the conversion always positive
                int iL =  localPower + ((*sp) * localPower + .5);
                int iR =  localPower + ((*(sp + 1)) * localPower + .5);
                localv0L = localStep * (((float)iL) - localPower);
                localv0R = localStep * (((float)iR) - localPower);
            }

            *sp++ = localv0L * mixerGain;
            *sp++ = localv0R * mixerGain;
        }
        v0L = localv0L;
        v0R = localv0R;
        fxPhase = localPhase;

    }
    break;
    case FILTER_BP:
    {
//        float input;                    // input sample
//        float output;                   // output sample
//        float v;                        // This is the intermediate value that
//                                        //    gets stored in the delay registers
//        float old1;                     // delay register 1, initialized to 0
//        float old2;                     // delay register 2, initialized to 0
//
//        /* filter coefficients */
//        omega1  = 2 * PI * f/srate; // f is your center frequency
//        sn1 = (float)sin(omega1);
//        cs1 = (float)cos(omega1);
//        alpha1 = sn1/(2*Qvalue);        // Qvalue is none other than Q!
//        a0 = 1.0f + alpha1;     // a0
//        b0 = alpha1;            // b0
//        b1 = 0.0f;          // b1/b0
//        b2= -alpha1/b0          // b2/b0
//        a1= -2.0f * cs1/a0;     // a1/a0
//        a2= (1.0f - alpha1)/a0;          // a2/a0
//        k = b0/a0;
//
//        /* The filter code */
//
//        v = k*input - a1*old1 - a2*old2;
//        output = v + b1*old1 + b2*old2;
//        old2 = old1;
//        old1 = v;

        // fxParam1 v
        //

        float fxParam1PlusMatrixTmp = params.effect.param1 + matrix.getDestination(FILTER_FREQUENCY);
        if (unlikely(fxParam1PlusMatrixTmp > 1.0f)) {
            fxParam1PlusMatrixTmp = 1.0f;
        }
        if (unlikely(fxParam1PlusMatrixTmp < 0.0f)) {
            fxParam1PlusMatrixTmp = 0.0f;
        }

        if (fxParam1PlusMatrix != fxParam1PlusMatrixTmp) {
            fxParam1PlusMatrix = fxParam1PlusMatrixTmp;
            recomputeBPValues();
        }

        float localv0L = v0L;
        float localv0R = v0R;
        float localv1L = v1L;
        float localv1R = v1R;
        float *sp = this->sampleBlock;

        for (int k=0 ; k < BLOCK_SIZE ; k++) {
            float localV = fxParam1 /* k */ * (*sp) - fxParamA1 * localv0L - fxParamA2 * localv1L;
            *sp++ = (localV + /* fxParamB1 (==0) * localv0L  + */ fxParamB2 * localv1L) * mixerGain;
            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }
            localv1L = localv0L;
            localv0L = localV;

            localV = fxParam1 /* k */ * (*sp) - fxParamA1 * localv0R - fxParamA2 * localv1R;
            *sp++ = (localV + /* fxParamB1 (==0) * localv0R + */ fxParamB2 * localv1R) * mixerGain;
            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }
            localv1R = localv0R;
            localv0R = localV;


        }

        v0L = localv0L;
        v0R = localv0R;
        v1L = localv1L;
        v1R = localv1R;

        break;
    }
	//### ADDED ###
	case FILTER_FOLDER:
	{
    	float *sp = this->sampleBlock;
		float val;
        float sampleL;
        float sampleR;
    	for (int k=0 ; k < BLOCK_SIZE ; k++) {
			sampleL = *(sp);
			sampleR = *(sp + 1);
			// shift
			sampleL = sampleL + fxParam1;
			// stretch
			sampleL = sampleL * fxParam2;
			// fold
			if(sampleL > 0)	// positive half
			{
				val = fmod(sampleL, 1.0f);
				if(fmod(sampleL, 2.0f) >= 1)	// fold back
					sampleL = 1.0f - val;
				else
					sampleL = val;
			}
			else			// negative half
			{
				val = -fmod(sampleL, 1.0f);
				if(fmod(sampleL, 2.0f) < -1)	// fold back
					sampleL = val - 1.0f;
				else
					sampleL = -val;
			}
    		// final gain - necessary?
			*sp++ = sampleL * mixerGain;

			// shift
			sampleR = sampleR + fxParam1;
			// stretch
			sampleR = sampleR * fxParam2;
			// fold
			if(sampleR > 0)	// positive half
			{
				val = fmod(sampleR, 1.0f);
				if(fmod(sampleR, 2.0f) >= 1)	// fold back
					sampleR = 1.0f - val;
				else
					sampleR = val;
			}
			else			// negative half
			{
				val = -fmod(sampleR, 1.0f);
				if(fmod(sampleR, 2.0f) < -1)	// fold back
					sampleR = val - 1.0f;
				else
					sampleR = -val;
			}
    		// final gain - necessary?
			*sp++ = sampleR * mixerGain;
		}
	}
	break;
	//#############
    case FILTER_OFF:
    {
    	// Filter off has gain...
    	float *sp = this->sampleBlock;
    	for (int k=0 ; k < BLOCK_SIZE ; k++) {
			*sp++ = (*sp) * mixerGain;
			*sp++ = (*sp) * mixerGain;
		}
    }
    break;

    default:
    	// NO EFFECT
   	break;
    }

}


void Timbre::afterNewParamsLoad() {
    this->matrix.resetSources();
    this->matrix.resetAllDestination();
	 for (int j=0; j<NUMBER_OF_ENCODERS * 2; j++) {
		this->env1.reloadADSR(j);
		this->env2.reloadADSR(j);
		this->env3.reloadADSR(j);
		this->env4.reloadADSR(j);
		this->env5.reloadADSR(j);
		this->env6.reloadADSR(j);
	}

	for (int j=0; j<NUMBER_OF_ENCODERS; j++) {
		this->lfoOsc[0].valueChanged(j);
		this->lfoOsc[1].valueChanged(j);
		this->lfoOsc[2].valueChanged(j);
		this->lfoEnv[0].valueChanged(j);
		this->lfoEnv2[0].valueChanged(j);
		this->lfoStepSeq[0].valueChanged(j);
		this->lfoStepSeq[1].valueChanged(j);
	}

    resetArpeggiator();
    v0L = v1L = 0.0f;
    v0R = v1R = 0.0f;
    for (int k=0; k<NUMBER_OF_ENCODERS; k++) {
    	setNewEffecParam(k);
    }
	//### ADDED ###
	if(timbreNumber == 0)
	{
		// update env2 oscillator
		lfoEnv[0].updateOscValues();
		lfoEnv2[0].updateOscValues();

		// which step seq into which step oscillator
		this->lfoStepSeq[0].updateOscValues(0);
		this->lfoStepSeq[1].updateOscValues(1);
	}
	//#############
}


void Timbre::resetArpeggiator() {
	// Reset Arpeggiator
	FlushQueue();
	note_stack.Clear();
	setArpeggiatorClock(params.engineArp1.clock);
	setLatchMode(params.engineArp2.latche);
}


void Timbre::setNewValue(int index, struct ParameterDisplay* param, float newValue) {
    if (newValue > param->maxValue) {
        newValue= param->maxValue;
    } else if (newValue < param->minValue) {
        newValue= param->minValue;
    }
    ((float*)&params)[index] = newValue;
}

void Timbre::recomputeBPValues() {
    //        /* filter coefficients */
    //        omega1  = 2 * PI * f/srate; // f is your center frequency
    //        sn1 = (float)sin(omega1);
    //        cs1 = (float)cos(omega1);
    //        alpha1 = sn1/(2*Qvalue);        // Qvalue is none other than Q!
    //        a0 = 1.0f + alpha1;     // a0
    //        b0 = alpha1;            // b0
    //        b1 = 0.0f;          // b1/b0
    //        b2= -alpha1/b0          // b2/b0
    //        a1= -2.0f * cs1/a0;     // a1/a0
    //        a2= (1.0f - alpha1)/a0;          // a2/a0
    //        k = b0/a0;

    // frequency must be up to SR / 2.... So 1024 * param1 :
    // 1000 instead of 1024 to get rid of strange border effect....
    float param1Square = fxParam1PlusMatrix * fxParam1PlusMatrix;
    float sn1 = sinTable[(int)(12 + 1000 * param1Square)];
    // sin(x) = cos( PI/2 - x)
    int cosPhase = 500 - 1000 * param1Square;
    if (cosPhase < 0) {
        cosPhase += 2048;
    }
    float cs1 = sinTable[cosPhase];

    float alpha1 = sn1 * 12.5;
    if (params.effect.param2 > 0) {
        alpha1 = sn1 / ( 8 * params.effect.param2);
    }

    float A0 = 1.0f + alpha1;
    float A0Inv = 1 / A0;

    float B0 = alpha1;
    fxParamB1 = 0.0;
    fxParamB2 = - alpha1 * A0Inv;
    fxParamA1 = -2.0f * cs1 * A0Inv;
    fxParamA2 = (1.0f - alpha1) * A0Inv;

    fxParam1 = B0 * A0Inv;
}

void Timbre::setNewEffecParam(int encoder) {
	if (encoder == 0) {
	    v0L = v1L = 0.0f;
	    v0R = v1R = 0.0f;
	    for (int k=1; k<NUMBER_OF_ENCODERS; k++) {
	        setNewEffecParam(k);
	    }
	}
	switch ((int)params.effect.type) {
	case FILTER_BASS:
		// Selectivity = fxParam1
		// ratio = fxParam2
		// gain1 = fxParam3
		fxParam1 = 50 + 200 * params.effect.param1;
		fxParam2 = params.effect.param2 * 4;
		fxParam3 = 1.0/(fxParam1 + 1.0);
		break;
	case FILTER_HP:
	case FILTER_LP:
		switch (encoder) {
		case ENCODER_EFFECT_TYPE:
			fxParam2 = 0.3f - params.effect.param2 * 0.3f;
			break;
		case ENCODER_EFFECT_PARAM1:
			// Done in every loop
			// fxParam1 = pow(0.5, (128- (params.effect.param1 * 128))   / 16.0);
			break;
		case ENCODER_EFFECT_PARAM2:
	    	// fxParam2 = pow(0.5, ((params.effect.param2 * 127)+24) / 16.0);
			// => value from 0.35 to 0.0
			fxParam2 = 0.27f - params.effect.param2 * 0.27f;
			break;
		}
    	break;
    case FILTER_CRUSHER:
    {
        int fxBit = 1.0f + 15.0f * params.effect.param2;
        fxParam1 = pow(2, fxBit);
        fxParam2 = 1 / fxParam1;
        break;
    }
    case FILTER_BP:
    {
        fxParam1PlusMatrix = -1.0;
        break;
    }
	//### ADDED ###
	case FILTER_FOLDER:
	{
		// shift up past limit (0 -> 2)
		fxParam1 = params.effect.param1 + params.effect.param1;
		// stretch up to 5 times...
		fxParam2 = (params.effect.param2 * 4) + 1.0f;
		break;
	}
	//#############
	}
}


// Code bellowed have been adapted by Xavier Hosxe for PreenFM2
// It come from Muteable Instrument midiPAL

/////////////////////////////////////////////////////////////////////////////////
// Copyright 2011 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// Arpeggiator app.



void Timbre::arpeggiatorNoteOn(char note, char velocity) {
	// CLOCK_MODE_INTERNAL
	if (params.engineArp1.clock == CLOCK_INTERNAL) {
		if (idle_ticks_ >= 96 || !running_) {
			Start();
		}
		idle_ticks_ = 0;
	}

	if (latch_ && !recording_) {
		note_stack.Clear();
		recording_ = 1;
	}
	note_stack.NoteOn(note, velocity);
}


void Timbre::arpeggiatorNoteOff(char note) {
	if (ignore_note_off_messages_) {
		return;
	}
	if (!latch_) {
		note_stack.NoteOff(note);
	} else {
		if (note == note_stack.most_recent_note().note) {
			recording_ = 0;
		}
	}
}


void Timbre::OnMidiContinue() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL) {
		running_ = 1;
	}
}

void Timbre::OnMidiStart() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL) {
		Start();
	}
}

void Timbre::OnMidiStop() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL) {
		running_ = 0;
		FlushQueue();
	}
}


void Timbre::OnMidiClock() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL && running_) {
		Tick();
	}
}


void Timbre::SendNote(uint8_t note, uint8_t velocity) {

	// If there are some Note Off messages for the note about to be triggeered
	// remove them from the queue and process them now.
	if (event_scheduler.Remove(note, 0)) {
		preenNoteOff(note);
	}

	// Send a note on and schedule a note off later.
	preenNoteOn(note, velocity);
	event_scheduler.Schedule(note, 0, midi_clock_tick_per_step[(int)params.engineArp2.duration] - 1, 0);
}

void Timbre::SendLater(uint8_t note, uint8_t velocity, uint8_t when, uint8_t tag) {
	event_scheduler.Schedule(note, velocity, when, tag);
}


void Timbre::SendScheduledNotes() {
  uint8_t current = event_scheduler.root();
  while (current) {
    const SchedulerEntry& entry = event_scheduler.entry(current);
    if (entry.when) {
      break;
    }
    if (entry.note != kZombieSlot) {
      if (entry.velocity == 0) {
    	  preenNoteOff(entry.note);
      } else {
    	  preenNoteOn(entry.note, entry.velocity);
      }
    }
    current = entry.next;
  }
  event_scheduler.Tick();
}


void Timbre::FlushQueue() {
  while (event_scheduler.size()) {
    SendScheduledNotes();
  }
}



void Timbre::Tick() {
	++tick_;

	if (note_stack.size()) {
		idle_ticks_ = 0;
	}
	++idle_ticks_;
	if (idle_ticks_ >= 96) {
		idle_ticks_ = 96;
	    if (params.engineArp1.clock == CLOCK_INTERNAL) {
	      running_ = 0;
	      FlushQueue();
	    }
	}

	SendScheduledNotes();

	if (tick_ >= midi_clock_tick_per_step[(int)params.engineArp2.division]) {
		tick_ = 0;
		uint16_t pattern = getArpeggiatorPattern();
		uint8_t has_arpeggiator_note = (bitmask_ & pattern) ? 255 : 0;
		const int num_notes = note_stack.size();
		const int direction = params.engineArp1.direction;

		if (num_notes && has_arpeggiator_note) {
			if ( ARPEGGIO_DIRECTION_CHORD != direction ) {
				StepArpeggio();
				int step, transpose = 0;
				if ( current_direction_ > 0 ) {
					step = start_step_ + current_step_;
					if ( step >= num_notes ) {
						step -= num_notes;
						transpose = 12;
					}
				} else {
					step = (num_notes - 1) - (start_step_ + current_step_);
					if ( step < 0 ) {
						step += num_notes;
						transpose = -12;
					}
				}
#ifdef DEBUG_ARP_STEP
				lcd.setRealTimeAction(true);
				lcd.setCursor(16,0);
				lcd.print( current_direction_ > 0 ? '+' : '-' );
				lcd.print( step );
				lcd.setRealTimeAction(false);
#endif
				const NoteEntry &noteEntry = ARPEGGIO_DIRECTION_PLAYED == direction
					? note_stack.played_note(step)
					: note_stack.sorted_note(step);

				uint8_t note = noteEntry.note;
				uint8_t velocity = noteEntry.velocity;
				note += 12 * current_octave_;
				if ( __canTranspose( direction ) )
					 note += transpose;

				while (note > 127) {
					note -= 12;
				}

				SendNote(note, velocity);
			} else {
				for (int i = 0; i < note_stack.size(); ++i ) {
					const NoteEntry& noteEntry = note_stack.sorted_note(i);
					SendNote(noteEntry.note, noteEntry.velocity);
				}
			}
		}
		bitmask_ <<= 1;
		if (!bitmask_) {
			bitmask_ = 1;
		}
	}
}



void Timbre::StepArpeggio() {

	if (current_octave_ == 127) {
		StartArpeggio();
		return;
	}

	int direction = params.engineArp1.direction;
	uint8_t num_notes = note_stack.size();
	if (direction == ARPEGGIO_DIRECTION_RANDOM) {
		uint8_t random_byte = *(uint8_t*)noise;
		current_octave_ = random_byte & 0xf;
		current_step_ = (random_byte & 0xf0) >> 4;
		while (current_octave_ >= params.engineArp1.octave) {
			current_octave_ -= params.engineArp1.octave;
		}
		while (current_step_ >= num_notes) {
			current_step_ -= num_notes;
		}
	} else {
		// NOTE: We always count [0 - num_notes) here; the actual handling of direction is in Tick()

		uint8_t trigger_change = 0;
		if (++current_step_ >= num_notes) {
			current_step_ = 0;
			trigger_change = 1;
		}

		// special case the 'ROTATE' and 'SHIFT' modes, they might not change the octave until the cycle is through
		if (trigger_change && (direction >= ARPEGGIO_DIRECTION_ROTATE_UP ) ) {
			if ( ++start_step_ >= num_notes )
				start_step_ = 0;
			else
				trigger_change = 0;
		}

		if (trigger_change) {
			current_octave_ += current_direction_;
			if (current_octave_ >= params.engineArp1.octave || current_octave_ < 0) {
				if ( __canChangeDir(direction) ) {
					current_direction_ = -current_direction_;
					StartArpeggio();
					if (num_notes > 1 || params.engineArp1.octave > 1) {
						StepArpeggio();
					}
				} else {
					StartArpeggio();
				}
			}
		}
	}
}

void Timbre::StartArpeggio() {

	current_step_ = 0;
	start_step_ = 0;
	if (current_direction_ == 1) {
		current_octave_ = 0;
	} else {
		current_octave_ = params.engineArp1.octave - 1;
	}
}

void Timbre::Start() {
	bitmask_ = 1;
	recording_ = 0;
	running_ = 1;
	tick_ = midi_clock_tick_per_step[(int)params.engineArp2.division] - 1;
    current_octave_ = 127;
	current_direction_ = __getDirection( params.engineArp1.direction );
}


void Timbre::arpeggiatorSetHoldPedal(uint8_t value) {
  if (ignore_note_off_messages_ && !value) {
    // Pedal was released, kill all pending arpeggios.
    note_stack.Clear();
  }
  ignore_note_off_messages_ = value;
}


void Timbre::setLatchMode(uint8_t value) {
    // When disabling latch mode, clear the note stack.
	latch_ = value;
    if (value == 0) {
      note_stack.Clear();
      recording_ = 0;
    }
}

void Timbre::setDirection(uint8_t value) {
	// When changing the arpeggio direction, reset the pattern.
	current_direction_ = __getDirection(value);
	StartArpeggio();
}

void Timbre::lfoValueChange(int currentRow, int encoder, float newValue) {
	switch (currentRow) {
	case ROW_LFOOSC1:
	case ROW_LFOOSC2:
	case ROW_LFOOSC3:
		lfoOsc[currentRow - ROW_LFOOSC1].valueChanged(encoder);
		break;
	case ROW_LFOENV1:
		lfoEnv[0].valueChanged(encoder);
		//### ADDED ###
		if(timbreNumber == 0)
		{
			// update env1 oscillator
			lfoEnv[0].updateOscValues();
		}
		//#############
		break;
	case ROW_LFOENV2:
		lfoEnv2[0].valueChanged(encoder);
		//### ADDED ###
		if(timbreNumber == 0)
		{
			// update env2 oscillator
			lfoEnv2[0].updateOscValues();
		}
		//#############
		break;
	case ROW_LFOSEQ1:
	case ROW_LFOSEQ2:
		lfoStepSeq[currentRow - ROW_LFOSEQ1].valueChanged(encoder);
		//### ADDED ###
		if(timbreNumber == 0 && encoder == 3)
		{
			// update step oscillator
			this->lfoStepSeq[currentRow - ROW_LFOSEQ1].updateOscValues(currentRow - ROW_LFOSEQ1);
		}
		//#############
		break;
	}
}


