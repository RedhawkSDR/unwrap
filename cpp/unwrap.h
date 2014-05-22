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
#ifndef UNWRAP_IMPL_H
#define UNWRAP_IMPL_H

#include "unwrap_base.h"
#include <map>

class unwrap_i;

class unwrap_i : public unwrap_base
{
    ENABLE_LOGGING
    public:
        unwrap_i(const char *uuid, const char *label);
        ~unwrap_i();
        int serviceFunction();
    private:
        //function pointer used to map complex data as per user request
        //to allow for more flexibility with different complex to real transformations
        //although "phase" is probably the most useful
        typedef float (*t_cxFunc)(const std::complex<float>&);

        void mapCxFunc();
        void updateValues();

        void toggleValsChanged(const float* newVal, const float* oldVal);
        void toggleUpdateCxOperator(const std::string* oldVal, const std::string* newVal);

        std::map<std::string, bool> isComplex;	//data structure to tell which streams are complex
        std::map<std::string, float> last;		//data structure to hold the last value for each stream

        std::vector<float> realVec;		//vector of real data to store the transfomred data if input is complex
    	std::vector<float>* unwrapVec;	//pointer for our data to unwrap

    	float maxVal;     		//max of Val1, Val2
    	float minVal;      		//min of Val2, Val2
    	float difference;  		//maxVal-minVal
    	float half_difference; 	//difference/2.0
    	t_cxFunc cxFunc;   		//which complex transforamtion are we using?
    	bool newCxFunc;			//flag for a new complex function
    	bool newValues;			//flag for newValues
};

#endif
