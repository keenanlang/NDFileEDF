#include <fstream>
#include <asynDriver.h>
#include <NDPluginFile.h>
#include <NDArray.h>

#define NDFileEDFMinHeaderStr  "EDF_MIN_HEADER"


class epicsShareClass NDFileEDF : public NDPluginFile
{
  public:
    NDFileEDF(const char *portName, int queueSize, int blockingCallbacks, 
               const char *NDArrayPort, int NDArrayAddr,
               int priority, int stackSize);
       
    /* The methods that this class implements */
    virtual asynStatus openFile(const char *fileName, NDFileOpenMode_t openMode, NDArray *pArray);
    virtual asynStatus readFile(NDArray **pArray);
    virtual asynStatus writeFile(NDArray *pArray);
    virtual asynStatus closeFile();
	
  protected:
    /* plugin parameters */
	int NDFileEDFMinHeader;

  private:
	std::ofstream file;
};
#define NUM_NDFILE_RAW_PARAMS 1
