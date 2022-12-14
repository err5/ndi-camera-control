/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#pragma once
#include "CHOP_CPlusPlusBase.h"

#include "Processing.NDI.Lib.h"
#include "..\Examples\C++\NDIlib_Send_VirtualPTZ\rapidxml\rapidxml.hpp"
#ifdef _WIN32
#define strcasecmp _stricmp
#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64
#else
#include <strings.h>
#endif // _WIN32

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <assert.h>

/*

This example file implements a class that does 2 different things depending on
if a CHOP is connected to the CPlusPlus CHOPs input or not.
The example is timesliced, which is the more complex way of working.

If an input is connected the node will output the same number of channels as the
input and divide the first 'N' samples in the input channel by 2. 'N' being the current
timeslice size. This is noteworthy because if the input isn't changing then the output
will look wierd since depending on the timeslice size some number of the first samples
of the input will get used.

If no input is connected then the node will output a smooth sine wave at 120hz.
*/

struct CameraData {
	double abs_pan;
	double abs_tilt;
	double abs_zoom;
	double abs_focus;

	double speed_pan;
	double speed_tilt;
	double speed_zoom;
	double speed_focus;

	double gain;
	double iris;
	double shutter_speed;
};

// To get more help about these functions, look at CHOP_CPlusPlusBase.h
class NDI_CameraControl_CHOP : public CHOP_CPlusPlusBase
{
public:
	NDI_CameraControl_CHOP(const OP_NodeInfo* info);
	virtual ~NDI_CameraControl_CHOP();

	virtual void		getGeneralInfo(CHOP_GeneralInfo*, const OP_Inputs*, void*) override;
	virtual bool		getOutputInfo(CHOP_OutputInfo*, const OP_Inputs*, void*) override;
	virtual void		getChannelName(int32_t index, OP_String* name, const OP_Inputs*, void* reserved) override;

	virtual void		execute(CHOP_Output*,
		const OP_Inputs*,
		void* reserved) override;


	virtual int32_t		getNumInfoCHOPChans(void* reserved1) override;
	virtual void		getInfoCHOPChan(int index,
		OP_InfoCHOPChan* chan,
		void* reserved1) override;

	virtual bool		getInfoDATSize(OP_InfoDATSize* infoSize, void* resereved1) override;
	virtual void		getInfoDATEntries(int32_t index,
		int32_t nEntries,
		OP_InfoDATEntries* entries,
		void* reserved1) override;

	virtual void		setupParameters(OP_ParameterManager* manager, void* reserved1) override;
	virtual void		pulsePressed(const char* name, void* reserved1) override;

	virtual void NDI_CameraControl_CHOP::UpdateSources();
	virtual void NDI_CameraControl_CHOP::ConnectByURL(const char* camera_url);
	virtual void NDI_CameraControl_CHOP::ConnectByID(int ID);

	// NDI Finder
private:

	// We don't need to store this pointer, but we do for the example.
	// The OP_NodeInfo class store information about the node that's using
	// this instance of the class (like its name).
	const OP_NodeInfo* myNodeInfo;

	// In this example this value will be incremented each time the execute()
	// function is called, then passes back to the CHOP 
	int32_t				myExecuteCount;

	double				myOffset;

	// NDI specific stuff
	const NDIlib_source_t* p_sources;
	NDIlib_find_instance_t pNDI_find;
	NDIlib_recv_create_v3_t NDI_recv_create_desc;
	NDIlib_recv_instance_t pNDI_recv;

	// list of all NDI source names & ips 
	// array size is arbitrary
	const char* source_names[256];
	const char* source_ips[256];

	char* selected_id_old = 0;

	CameraData cam_data = {};

};
