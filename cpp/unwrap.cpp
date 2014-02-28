/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This file is part of REDHAWK Basic Components unwrap.
 *
 * REDHAWK Basic Components HardLimit is free software: you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * REDHAWK Basic Components HardLimit is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this
 * program.  If not, see http://www.gnu.org/licenses/.
 */
/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

**************************************************************************/

#include "unwrap.h"
#include <cmath>
#include <complex>

PREPARE_LOGGING(unwrap_i)

unwrap_i::unwrap_i(const char *uuid, const char *label) :
    unwrap_base(uuid, label)

{
	//initialize our internal data members
	mapCxFunc();
	updateValues();
	//update our property callbacks
	setPropertyChangeListener("Val1", this, &unwrap_i::toggleValsChanged);
	setPropertyChangeListener("Val2", this, &unwrap_i::toggleValsChanged);
	setPropertyChangeListener("cxOperator", this, &unwrap_i::toggleUpdateCxOperator);
}

unwrap_i::~unwrap_i()
{
}


float realFunc(const std::complex<float>& val)
{
	return val.real();
}
float imagFunc(const std::complex<float>& val)
{
	return val.imag();
}

void unwrap_i::mapCxFunc()
{
	//choose our complex to real map in case use pases us complex data
	if (cxOperator=="phase")
		cxFunc=&std::arg;
	else if (cxOperator=="abs")
		cxFunc=&std::abs;
	else if (cxOperator=="norm")
		cxFunc=&std::norm;
	else if (cxOperator=="real")
		cxFunc=&realFunc;
	else if (cxOperator=="imag")
		cxFunc=&imagFunc;
	newCxFunc=false;
}

void unwrap_i::updateValues()
{
	//update our internal data values
	maxVal = std::max(Val1,Val2);
	minVal = std::min(Val1,Val2);
	difference = maxVal-minVal;
	half_difference = difference/2.0;
	newValues = false;
}

void unwrap_i::toggleValsChanged(const std::string&)
{
	//property callback - tell the service function to update its newValues nextLoop
	newValues=true;
}
void unwrap_i::toggleUpdateCxOperator(const std::string&)
{
	//property callback - tell the service function to check for a new complex funtion
	newCxFunc=true;
}

int unwrap_i::serviceFunction()
{
	//main processing loop
	LOG_DEBUG(unwrap_i, "serviceFunction() example log message");
	//get data from the port
	bulkio::InFloatPort::dataTransfer *tmp = dataFloat_in->getPacket(bulkio::Const::BLOCKING);
	if (not tmp) { // No data is available
		return NOOP;
	}
	if (tmp->inputQueueFlushed)
	{
		LOG_WARN(unwrap_i, "input queue flushed - data has been thrown on the floor.  flushing internal buffers");
		//flush all our processor states if the queue flushed
	    isComplex.clear();	//data structure to tell which streams are complex
	    last.clear();		//data structure to hold the last value for each stream
	}

	//find out if we are complex our not
	std::map<std::string, bool>::iterator isCxIter(isComplex.find(tmp->streamID));
    std::pair<std::string, bool> isCxPair;
	if (tmp->sriChanged or (isCxIter == isComplex.end())) {
		//we haven't seen this stream before or sri has changed
		isCxPair.second = (tmp->SRI.mode!=0);
		//store off our state for next go if this isn't the end of stream
		if (!tmp->EOS)
		{
			if (isCxIter != isComplex.end())
			{
				//if we already have this stream update its value with our current state
				isCxIter->second =isCxPair.second;
			}
			else
			{
				//this is a new stream - set the stream ID of our pair and insert it into the map
				isCxPair.first =  tmp->streamID;
				isComplex.insert(isCxPair);
			}
		}
		//output of this component is always real - update SRI before push
		tmp->SRI.mode=0;
		// push SRI
		// NOTE: You must make at least one valid pushSRI call
		dataFloat_out->pushSRI(tmp->SRI);
	}
	else
		isCxPair.second = isCxIter->second;

	if (!tmp->dataBuffer.empty())
	{
		//we have data -time to do the work

		//if data is complex we need to transform it to real data before doing our unwrapping
		if (isCxPair.second)
		{
			//data is complex
			std::vector<std::complex<float> >* cxVec = (std::vector<std::complex<float> >*) &(tmp->dataBuffer);
			//store the transform of the complex vector into the realVec
			unwrapVec = &realVec;
			realVec.clear();
			realVec.reserve(cxVec->size());
			//update our complex map function if requried
			if (newCxFunc)
				mapCxFunc();
			//now transform the data to real using the appropraite cxFunc
			for (std::vector<std::complex<float> >::iterator i  = cxVec->begin(); i!= cxVec->end(); i++)
				realVec.push_back((*cxFunc)(*i));
		}
		else
			//data is already real - just use the input as is
			unwrapVec = &tmp->dataBuffer;

		//get the value from the last push if it exists -- otherwise initialize to the first value of this vector
		float lastVal;
		std::map<std::string, float>::iterator lastIter = last.find(tmp->streamID);
		if (lastIter!=last.end())
			lastVal =lastIter->second;
		else
			lastVal =unwrapVec->front();

		//update ranges for our unwraping if required
		if (newValues)
			updateValues();

		//now do the unwrap algorithm

		//Intiuitively - we are defining the principalVal as the value where
		// such that minVal <=principalValue<=maxVal
		// and principalValue = value - i*difference for some integer i
		// in other words - the principle value is the current value "wrapped" into the range [-minVal, maxVal]

		// we compare the current and last principle value
		// if the absolute value of the difference between them > 1/2 the difference between the min and max value
		// then we are actually closer to an adjucent wrapped value
		// We update the wrap index and re-map the output accordingly

		//how many multiples of difference are we away from [minVal,maxVal]
		long wrapIndex = (lastVal-minVal)/difference;
		// initialize lastPriniciple as the principle value of the lastVal
		float lastPrincipal = lastVal-(wrapIndex*difference);
		float principalVal;
		for (std::vector<float>::iterator i  = unwrapVec->begin(); i!= unwrapVec->end(); i++)
		{
			//ideally all input values would already be principalValues - put this calculation here to enforce that constraint
			//by mapping our value to its principle value equvilient
			principalVal = fmod(*i-minVal,difference)+minVal;
			//now we compare the two principle values and decide if the new value has "wrapped" over
			if ((principalVal-lastPrincipal) > half_difference)
			{
				wrapIndex--;
			}
			else if ((lastPrincipal-principalVal) > half_difference)
			{
				wrapIndex++;
			}
			//update the prinicpal value for next sample
			lastPrincipal=principalVal;
			//calculate the output value given our current principalValue and our current wrapIndex
			*i = principalVal + difference*wrapIndex;
		}
		//done with calculations
		if (! tmp->EOS)
			//copy the last value for next push
			last[tmp->streamID]=unwrapVec->back();
		else
		{
			//if we are done with this stream clear our state
			if (isCxIter!= isComplex.end())
				isComplex.erase(isCxIter);
			if (lastIter!=last.end())
				last.erase(lastIter);
		}
		//output the data
		dataFloat_out->pushPacket(*unwrapVec, tmp->T, tmp->EOS, tmp->streamID);
	}
	//done
	delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
	return NORMAL;

}
