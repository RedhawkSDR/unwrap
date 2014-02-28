#!/usr/bin/env python
#
# This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this 
# source distribution.
# 
# This file is part of REDHAWK Basic Components unwrap.
# 
# REDHAWK Basic Components HardLimit is free software: you can redistribute it and/or modify it under the terms of 
# the GNU Lesser General Public License as published by the Free Software Foundation, either 
# version 3 of the License, or (at your option) any later version.
# 
# REDHAWK Basic Components HardLimit is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
# PURPOSE.  See the GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License along with this 
# program.  If not, see http://www.gnu.org/licenses/.
#
import unittest
import ossie.utils.testing
import os
from omniORB import any
from ossie.utils import sb
import time
import math

class ComponentTests(ossie.utils.testing.ScaComponentTestCase):
    """Test for all component implementations in unwrap"""

    def setUp(self):
        """Set up the unit test - this is run before every method that starts with test
        """
        ossie.utils.testing.ScaComponentTestCase.setUp(self)
        self.src = sb.DataSource()
        self.sink = sb.DataSink()
        
        #setup my components
        self.setupComponent()
        
        self.comp.start()
        self.src.start()
        self.sink.start()
        
        #do the connections
        self.src.connect(self.comp)        
        self.comp.connect(self.sink)
        
    def tearDown(self):
        """Finish the unit test - this is run after every method that starts with test
        """
        self.comp.stop()
        #######################################################################
        # Simulate regular component shutdown
        self.comp.releaseObject()
        self.sink.stop()      
        ossie.utils.testing.ScaComponentTestCase.tearDown(self)

    def setupComponent(self):
        #######################################################################
        # Launch the component with the default execparams
        execparams = self.getPropertySet(kinds=("execparam",), modes=("readwrite", "writeonly"), includeNil=False)
        execparams = dict([(x.id, any.from_any(x.value)) for x in execparams])
        self.launch(execparams)
        
        #######################################################################
        # Verify the basic state of the component
        self.assertNotEqual(self.comp, None)
        self.assertEqual(self.comp.ref._non_existent(), False)
        self.assertEqual(self.comp.ref._is_a("IDL:CF/Resource:1.0"), True)
        
        #######################################################################
        # Validate that query returns all expected parameters
        # Query of '[]' should return the following set of properties
        expectedProps = []
        expectedProps.extend(self.getPropertySet(kinds=("configure", "execparam"), modes=("readwrite", "readonly"), includeNil=True))
        expectedProps.extend(self.getPropertySet(kinds=("allocate",), action="external", includeNil=True))
        props = self.comp.query([])
        props = dict((x.id, any.from_any(x.value)) for x in props)
        # Query may return more than expected, but not less
        for expectedProp in expectedProps:
            self.assertEquals(props.has_key(expectedProp.id), True)
        
        #######################################################################
        # Verify that all expected ports are available
        for port in self.scd.get_componentfeatures().get_ports().get_uses():
            port_obj = self.comp.getPort(str(port.get_usesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a("IDL:CF/Port:1.0"),  True)
            
        for port in self.scd.get_componentfeatures().get_ports().get_provides():
            port_obj = self.comp.getPort(str(port.get_providesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a(port.get_repid()),  True)

    def main(self,inData, numPushes = 1, complexData = True):
        """The main engine for all the test cases - push data, and get output
           As applicable
        """
        #data processing is asynchronos - so wait until the data is all processed
        count=0
        startIndex=0
        numElements = len(inData)/numPushes
        if complexData and numElements%1==1:
            numElements+=1 
        for i in xrange(numPushes):
            endIndex = startIndex+numElements
            pushData =inData[startIndex:endIndex]
#            print " startIndex = ", startIndex, " endIndex = ", endIndex, "len = ", len(pushData),  "pushingData = ", pushData
            if pushData:
                self.src.push(pushData,
                      complexData = complexData)
            startIndex = endIndex
        out=[]
        while True:
            newOut = self.sink.getData()
            if newOut:
                out.extend(newOut)
                count=0
            elif count==100:
                break
            time.sleep(.01)
            count+=1
                      
        return out
        
        
    # TODO Add additional tests here
    #
    # See:
    #   ossie.utils.bulkio.bulkio_helpers,
    #   ossie.utils.bluefile.bluefile_helpers
    # for modules that will assist with testing components with BULKIO ports

    #test wrapping between +/pi
    def testOne(self):
        self.realTest(1, -3,5)
    def testTwo(self):
        self.realTest(1, 3,5)
    
    def testThree(self):
        self.realTest(-1, -3,5)
    def testFour(self):
        self.realTest(-1, -3,5)
#          
#     #test wrapping between 100 and 200
    def testFive(self):
        self.comp.Val1=100
        self.comp.Val2=200
        self.realTest(7.874,150, 5)
    def testSix(self):
        self.comp.Val1=100
        self.comp.Val2=200
        self.realTest(-7.874,150, 5)
   
    #test complex (real transformation) with wrapping between +/pi  
    def testSeven(self):
        self.cxTest(1, -3,5, 'real')
    def testEight(self):
        self.cxTest(1, 3,5, 'real')
     
    def testNine(self):
        self.cxTest(-1, -3,5, 'real')
    def testTen(self):
        self.cxTest(-1, -3,5, 'real')

    #test complex (phase transformation) with wrapping between +/pi  
    def testElevn(self):
        self.cxTest(1, -3,5, 'phase')
    def testTwelve(self):
        self.cxTest(1, 3,5, 'phase')
    
    def testThirteen(self):
        self.cxTest(-1, -3,5, 'phase')
    def testFourteen(self):
        self.cxTest(-1, -3,5, 'phase')
    
    def realTest(self, stepVal, initialVal, numPushes=1):
        """create real linear data  clipped between Val1 and Val2.  Push through the system and verify it is linear with no clipping
           (IE - the data has been unwrapped)
        """
        inData=[]
        vals = [self.comp.Val1, self.comp.Val2]
        vals.sort()
        minVal,maxVal = vals
        x=initialVal
        diff = maxVal-minVal
        numVals = 1000
        for _ in xrange(numVals):
            if x > maxVal:
                x-=diff
            if x < minVal:
                x+=diff
            inData.append(float(x))
            x+=stepVal
        out = self.main(inData,numPushes, False)
        assert(len(out)== numVals)
        diff = [x-y for x,y in zip(out[1:],out)]
        assert(all([abs(x-stepVal)<.01 for x in  diff]))

    def cxTest(self, stepValOut, initialVal, numPushes=1,cxMap='real'):
        """create complex data, push through the system and verify it is linear with no clipping
           (IE - the data has been unwrapped)
           for cxMap "real" - the data is real data with muxed 0s and is truncated betwen minVal and maxVal
           for cxMap "phase" - the data is a complex phasor (norm 1) which rotates with stepValOut.
           We don't need to clip the input data because phase is already ambigous and the real transform in the component 
           Results in clipped data
        """
        self.comp.cxOperator = cxMap
        inData=[]
        vals = [self.comp.Val1, self.comp.Val2]
        vals.sort()
        minVal,maxVal = vals
        diff = maxVal-minVal
        if cxMap=="real":
            x=initialVal
            stepVal = stepValOut
        elif cxMap =="phase":
            x=complex(math.cos(initialVal), math.sin(initialVal))
            stepVal= complex(math.cos(stepValOut), math.sin(stepValOut))
        numVals = 1000
        for _ in xrange(numVals):
            if cxMap=='real':
                if x > maxVal:
                    x-=diff
                if x < minVal:
                    x+=diff
                inData.append(float(x))
                inData.append(0)
                x+=stepVal
            elif cxMap=="phase":
                inData.append(x.real)
                inData.append(x.imag)
                x*=stepVal
                
        out = self.main(inData,numPushes, True)
        assert(len(out)== numVals)
        diff = [x-y for x,y in zip(out[1:],out)]
        assert(all([abs(x-stepValOut)<.01 for x in  diff]))


    
if __name__ == "__main__":
    ossie.utils.testing.main("../unwrap.spd.xml") # By default tests all implementations
