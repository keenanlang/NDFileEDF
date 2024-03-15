/* NDFileEDF.cpp
 * Writes NDArrays to raw files.
 *
 * Keenan Lang
 * October 5th, 2016
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsTime.h>
#include <iocsh.h>
#define epicsAssertAuthor "the EPICS areaDetector collaboration (https://github.com/areaDetector/ADCore/issues)"
#include <epicsAssert.h>

#include <asynDriver.h>

#include <epicsExport.h>
#include "NDFileEDF.h"


static const char *driverName = "NDFileEDF";


asynStatus NDFileEDF::openFile(const char *fileName, NDFileOpenMode_t openMode, NDArray *pArray)
{
	static const char *functionName = "openFile";

	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Filename: %s\n", driverName, functionName, fileName);

	// We don't support reading yet
	if (openMode & NDFileModeRead) 
	{
		setIntegerParam(NDFileCapture, 0);
		setIntegerParam(NDWriteFile, 0);
		return asynError;
	}

	// We don't support opening an existing file for appending yet
	if (openMode & NDFileModeAppend) 
	{
		setIntegerParam(NDFileCapture, 0);
		setIntegerParam(NDWriteFile, 0);
		return asynError;
	}

	// Check if an invalid (<0) number of frames has been configured for capture
	int numCapture;
	getIntegerParam(NDFileNumCapture, &numCapture);
	if (numCapture < 0) 
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
				  "%s::%s Invalid number of frames to capture: %d. Please specify a number >= 0\n",
				  driverName, functionName, numCapture);
		return asynError;
	}
	
	// Check to see if a file is already open and close it
	if (this->file.is_open())    { this->closeFile(); }

	// Create the new file
	this->file.open(fileName, std::ofstream::binary);
	
	if (! this->file.is_open())
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
				  "%s::%s ERROR Failed to create a new output file\n",
				  driverName, functionName);
		return asynError;
	}
	
	int min_header_size;
	getIntegerParam(NDFileEDFMinHeader, &min_header_size);
	
	//Write EDF header
	std::stringstream header_info;
	
	
	header_info << "\n{";
	header_info << "EDF_DataBlockID = 1.Image.Psd ; \r\n";
	header_info << "EDF_HeaderSize = ";
	
	
	std::stringstream output;
	
	output << "ByteOrder = LowByteFirst ; \r\n";
	
	output << "Num_Images = ";
	
	if (openMode & NDFileModeMultiple)    { output << numCapture; }
	else                                  { output << 1; }

	output << " ; \r\n";
	
	output << "DataType = ";
	
	switch (pArray->dataType)
	{
		case NDAttrInt8:
			output << "SignedByte";
			break;
			
		case NDAttrUInt8:
			output << "UnsignedByte";
			break;
			
		case NDAttrInt16:
			output << "SignedShort";
			break;
			
		case NDAttrUInt16:
			output << "UnsignedShort";
			break;
			
		case NDAttrInt32:
			output << "SignedInteger";
			break;
			
		case NDAttrUInt32:
			output << "UnsignedInteger";
			break;
			
		case NDAttrFloat32:
			output << "FloatValue";
			break;
			
		case NDAttrFloat64:
			output << "DoubleValue";
			break;
		
		default:
			output << "UnAssigned";
			break;
	}
	
	output << " ; \r\n";
	
	NDAttributeList* attrs = pArray->pAttributeList;
	NDAttribute* curr = NULL;
	
	while (attrs->next(curr) != NULL)
	{
		curr = attrs->next(curr);
		
		output << curr->getName();
		output << " = ";

		NDAttrValue value;
		
		switch (curr->getDataType())
		{
			case NDAttrInt8:
			case NDAttrUInt8:
			case NDAttrInt16:
			case NDAttrUInt16:
			case NDAttrInt32:
			case NDAttrUInt32:
			{
				curr->getValue(curr->getDataType(), &value.i32);
				output << value.i32;
				break;
			}
			
			case NDAttrFloat32:
			{
				curr->getValue(curr->getDataType(), &value.f32);
				output << value.f32;
				break;
			}
			
			
			case NDAttrFloat64:
			{
				curr->getValue(curr->getDataType(), &value.f64);
				output << value.f64;
				break;
			}
				
			case NDAttrString:
			{
				char buffer[256] = { '\0' };
				
				curr->getValue(curr->getDataType(), &buffer, sizeof(buffer));
				output << buffer;
				break;
			}
			
			default:
			{
				break;
			}
		}	
	
		output << " ; \r\n";
	}
	
	std::string first = header_info.str();
	std::string towrite = output.str();
	
	/* 
	 * Full header size is first's size plus towrite's size plus an extra 7 characters,
	 * but the overall size is dependent on the number of characters used to write the
	 * size of the header
	 */
	
	int initial_size = first.length() + towrite.length() + 7;
	
	int num_characters_in_size = 1 + (int) std::log10(initial_size);
	
	//Check if the extra characters in the header size are enough to make the header larger
	
	num_characters_in_size = 1 + (int) std::log10(initial_size + num_characters_in_size);
	
	int final_size = initial_size + num_characters_in_size;
	
	if (min_header_size != 0 && final_size < min_header_size)
	{
		final_size = min_header_size;
	
		header_info << min_header_size << " ; \r\n";
		std::string extra((size_t) (final_size - 2 - towrite.length()), '\x20');
		
		this->file << header_info.str();
		this->file << towrite;
		this->file << extra;
		this->file << "}\n";
	}
	else
	{
		header_info << final_size << " ; \r\n";
		this->file << header_info.str();
		this->file << towrite;
		this->file << "}\n";
	}
	
	return asynSuccess;
}

/** Writes NDArray data to a raw file.
  * \param[in] pArray Pointer to an NDArray to write to the file. This function can be called multiple
  *            times between the call to openFile and closeFile if NDFileModeMultiple was set in 
  *            openMode in the call to NDFileEDF::openFile.
  */
asynStatus NDFileEDF::writeFile(NDArray *pArray)
{
	asynStatus status = asynSuccess;
	static const char *functionName = "writeFile";

	if (! this->file.is_open())
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
				  "%s::%s file is not open!\n", 
				  driverName, functionName);
		return asynError;
	}

	NDArrayInfo info;
	pArray->getInfo(&info);
	
	this->file.write((const char*) pArray->pData, info.nElements * info.bytesPerElement);

	return asynSuccess;
}

/** Read NDArray data from a HDF5 file; NOTE: not implemented yet.
  * \param[in] pArray Pointer to the address of an NDArray to read the data into.  */ 
asynStatus NDFileEDF::readFile(NDArray **pArray)
{
  //static const char *functionName = "readFile";
  return asynError;
}

/** Closes the file opened with NDFileEDF::openFile 
 */ 
asynStatus NDFileEDF::closeFile()
{
	epicsInt32 numCaptured;
	static const char *functionName = "closeFile";

	if (!this->file.is_open())
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
				  "%s::%s file was not open! Ignoring close command.\n", 
				  driverName, functionName);
		return asynSuccess;
	}

	this->file.close();

	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s file closed!\n", driverName, functionName);


	return asynSuccess;
}


/** Constructor for NDFileEDF; parameters are identical to those for NDPluginFile::NDPluginFile,
    and are passed directly to that base class constructor.
  * After calling the base class constructor this method sets NDPluginFile::supportsMultipleArrays=1.
  */
NDFileEDF::NDFileEDF(const char *portName, int queueSize, int blockingCallbacks, 
                     const char *NDArrayPort, int NDArrayAddr,
                     int priority, int stackSize)
  /* Invoke the base class constructor.
   * We allocate 2 NDArrays of unlimited size in the NDArray pool.
   * This driver can block (because writing a file can be slow), and it is not multi-device.  
   * Set autoconnect to 1.  priority and stacksize can be 0, which will use defaults. */
  : NDPluginFile(portName, queueSize, blockingCallbacks,
                 NDArrayPort, NDArrayAddr, 1, NUM_NDFILE_RAW_PARAMS,
                 2, 0, asynGenericPointerMask, asynGenericPointerMask, 
                 ASYN_CANBLOCK, 1, priority, stackSize)
{
   this->createParam(NDFileEDFMinHeaderStr, asynParamInt32, &NDFileEDFMinHeader);
	
  //static const char *functionName = "NDFileEDF";
   setStringParam(NDPluginDriverPluginType, "NDFileEDF");

   this->supportsMultipleArrays = true;
}



/** Configuration routine. */
extern "C" int NDFileEDFConfigure(const char *portName, int queueSize, int blockingCallbacks, 
                                  const char *NDArrayPort, int NDArrayAddr,
                                  int priority, int stackSize)
{
  NDFileEDF* temp = new NDFileEDF(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr, priority, stackSize);
  
  return temp->start();
}


/** EPICS iocsh shell commands */
static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg initArg1 = { "frame queue size",iocshArgInt};
static const iocshArg initArg2 = { "blocking callbacks",iocshArgInt};
static const iocshArg initArg3 = { "NDArray Port",iocshArgString};
static const iocshArg initArg4 = { "NDArray Addr",iocshArgInt};
static const iocshArg initArg5 = { "priority",iocshArgInt};
static const iocshArg initArg6 = { "stack size",iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2,
                                            &initArg3,
                                            &initArg4,
                                            &initArg5,
                                            &initArg6};
static const iocshFuncDef initFuncDef = {"NDFileEDFConfigure",7,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
  NDFileEDFConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].sval, 
                      args[4].ival, args[5].ival, args[6].ival);
}

extern "C" void NDFileEDFRegister(void)
{
  iocshRegister(&initFuncDef,initCallFunc);
}

extern "C" {
epicsExportRegistrar(NDFileEDFRegister);
}

