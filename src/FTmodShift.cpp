/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#include "FTmodShift.hpp"
#include <cstdlib>
#include <cstdio>

using namespace std;
using namespace PBD;

FTmodShift::FTmodShift (nframes_t samplerate, unsigned int fftn)
	: FTmodulatorI ("Shift", samplerate, fftn)
{
	_confname = "Shift";
}

FTmodShift::FTmodShift (const FTmodShift & other)
	: FTmodulatorI ("Shift", other._sampleRate, other._fftN)
{
	_confname = "Shift";
}

void FTmodShift::initialize()
{
	_lastframe = 0;
	
	_rate = new Control (Control::FloatType, "Rate", "Units/sec");
	_rate->_floatLB = 0.0;
	_rate->_floatUB = 1000.0;
	_rate->setValue (500.0f);
	_controls.push_back (_rate);

	_dimension = new Control (Control::EnumType, "Axis", "");
	_dimension->_enumList.push_back("Frequency");
	_dimension->setValue ("Frequency");
	_controls.push_back (_dimension);

	_tmpfilt = new float[_fftN];
	
	_inited = true;
}

FTmodShift::~FTmodShift()
{
	if (!_inited) return;

	// need a going away signal
	
	_controls.clear();

	delete _rate;
	delete _dimension;
}

void FTmodShift::setFFTsize (unsigned int fftn)
{
	_fftN = fftn;

	if (_inited) {
		delete _tmpfilt;
		_tmpfilt = new float[_fftN];
	}
	
}


void FTmodShift::modulate (nframes_t current_frame, fft_data * fftdata, unsigned int fftn, sample_t * timedata, nframes_t nframes)
{
	TentativeLockMonitor lm (_specmodLock, __LINE__, __FILE__);

	if (!lm.locked() || !_inited || _bypassed) return;

	float rate = 1.0;
	float ub,lb;
	float * filter;
	int len;
	int i,j;
	
	_rate->getValue (rate);

	int shiftval = (int) (((current_frame - _lastframe) / (double) _sampleRate) * rate);
	
	if (current_frame != _lastframe && shiftval != 0)
	{
		// fprintf (stderr, "randomize at %lu :  samps=%g  s*c=%g  s*e=%g \n", (unsigned long) current_frame, samps, (current_frame/samps), ((current_frame + nframes)/samps) );

		
		for (SpecModList::iterator iter = _specMods.begin(); iter != _specMods.end(); ++iter)
		{
			FTspectrumModifier * sm = (*iter);
			if (sm->getBypassed()) continue;

			filter = sm->getValues();
			sm->getRange(lb, ub);
			len = (int) sm->getLength();
			int shiftbins = shiftval % len;

			// fprintf(stderr, "shifting %d at %lu\n", shiftbins, (unsigned long) current_frame);
			
			
			// shiftbins is POSITIVE, shift right
			// store last shiftbins
			for (i=len-shiftbins; i < len; i++) {
				_tmpfilt[i] = filter[i];
			}
			
			
			for ( i=len-1; i >= shiftbins; i--) {
				filter[i] = filter[i-shiftbins];
			}
			
			for (j=len-shiftbins, i=0; i < shiftbins; i++, j++) {
				filter[i] = _tmpfilt[j];
			}
			
			
			sm->setDirty(true);
		}

		_lastframe = current_frame;
	}
}