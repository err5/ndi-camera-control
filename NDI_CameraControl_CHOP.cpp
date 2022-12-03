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

/*
* // NDI PTZ Camera controller v0.1 \\
*	Made by Kostiantyn Yerokhin
*/

#include "NDI_CameraControl_CHOP.h"

extern "C"
{

	DLLEXPORT
		void
		FillCHOPPluginInfo(CHOP_PluginInfo* info)
	{
		// Always set this to CHOPCPlusPlusAPIVersion.
		info->apiVersion = CHOPCPlusPlusAPIVersion;

		// The opType is the unique name for this CHOP. It must start with a 
		// capital A-Z character, and all the following characters must lower case
		// or numbers (a-z, 0-9)
		info->customOPInfo.opType->setString("Customsignal");

		// The opLabel is the text that will show up in the OP Create Dialog
		info->customOPInfo.opLabel->setString("Custom Signal");

		// Information about the author of this OP
		info->customOPInfo.authorName->setString("Author Name");
		info->customOPInfo.authorEmail->setString("email@email.com");

		// This CHOP can work with 0 inputs
		info->customOPInfo.minInputs = 0;

		// It can accept up to 1 input though, which changes it's behavior
		info->customOPInfo.maxInputs = 1;
	}

	DLLEXPORT
		CHOP_CPlusPlusBase*
		CreateCHOPInstance(const OP_NodeInfo* info)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per CHOP that is using the .dll
		return new NDI_CameraControl_CHOP(info);
	}

	DLLEXPORT
		void
		DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the CHOP using that instance is deleted, or
		// if the CHOP loads a different DLL
		delete (NDI_CameraControl_CHOP*)instance;
	}

};

// Camera PTZ values will be initialized after first &::execute cycle
NDI_CameraControl_CHOP::NDI_CameraControl_CHOP(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	myOffset = 0.0;

	pNDI_find = NDIlib_find_create_v2();


	if (!NDIlib_initialize()) {
		// Cannot run NDI. Most likely because the CPU is not sufficient (see SDK
		// documentation). you can check this directly with a call to
		// NDIlib_is_supported_CPU()
		printf("Cannot run NDI.\n");
	}

	{
		if (!NDIlib_find_wait_for_sources(pNDI_find, 1000 /* milliseconds */)) {
			printf("No change to the sources found.\n");
		}

		// Get the updated list of sources
		uint32_t no_sources = 0;
		p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);

		
		NDI_recv_create_desc.source_to_connect_to = p_sources[0];
		NDI_recv_create_desc.p_ndi_recv_name = "TD->NDI Camera Controller";
		pNDI_recv = NDIlib_recv_create_v3(&NDI_recv_create_desc);
		if (!pNDI_recv) {
			printf("Error connecting to NDI source\n");
		}

		// Display all the sources.
		printf("Network sources (%u found).\n", no_sources);
		for (uint32_t i = 0; i < no_sources; i++) {
			printf("%u. %s\t\t", i + 1, p_sources[i].p_ndi_name);
			printf("%s\n", p_sources[i].p_url_address);

		}
	}
}

NDI_CameraControl_CHOP::~NDI_CameraControl_CHOP()
{
	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_find_destroy(pNDI_find);
	NDIlib_destroy();
}

void
NDI_CameraControl_CHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;

	// Note: To disable timeslicing you'll need to turn this off, as well as ensure that
	// getOutputInfo() returns true, and likely also set the info->numSamples to how many
	// samples you want to generate for this CHOP. Otherwise it'll take on length of the
	// input CHOP, which may be timesliced.
	ginfo->timeslice = true;

	ginfo->inputMatchIndex = 0;
}

bool
NDI_CameraControl_CHOP::getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs, void* reserved1)
{
	{
		info->numChannels = 4;

		// Since we are outputting a timeslice, the system will dictate
		// the numSamples and startIndex of the CHOP data
		info->numSamples = 0;
		info->startIndex = 0;

		// For illustration we are going to output 120hz data
		//info->sampleRate = 60;
		return true;
	}
}

void
NDI_CameraControl_CHOP::getChannelName(int32_t index, OP_String* name, const OP_Inputs* inputs, void* reserved1)
{
	switch (index) {
	case 0: {name->setString("abs_pan"); break; }
	case 1: {name->setString("abs_tilt"); break; }
	case 2: {name->setString("zoom"); break; }
	case 3: {name->setString("focus"); break; }
	}
}

void
NDI_CameraControl_CHOP::execute(CHOP_Output* output,
	const OP_Inputs* inputs,
	void* reserved)
{
	myExecuteCount++;
	
	inputs->enablePar("Cameraip", 1);
	inputs->enablePar("Abspan", 1);
	inputs->enablePar("Abstilt", 1);
	inputs->enablePar("Abszoom", 1);
	inputs->enablePar("Absfocus", 1);

	const char* camera_ip = inputs->getParString("Cameraip");
	double abs_pan_new = inputs->getParDouble("Abspan");
	double abs_tilt_new = inputs->getParDouble("Abstilt");
	double abs_zoom_new = inputs->getParDouble("Abszoom");
	double abs_focus_new = inputs->getParDouble("Absfocus");
	
	{

		if (abs_pan != abs_pan_new || abs_tilt != abs_tilt_new) {
			abs_pan = abs_pan_new;
			abs_tilt = abs_tilt_new;
			NDIlib_recv_ptz_pan_tilt_speed(pNDI_recv, abs_pan, abs_tilt);
		}
		if (abs_zoom != inputs->getParDouble("Abszoom")) {
			abs_zoom = abs_zoom_new;
			NDIlib_recv_ptz_zoom(pNDI_recv, abs_zoom);
		}
		if (abs_focus != inputs->getParDouble("Absfocus")) {
			abs_focus = abs_focus_new;
			NDIlib_recv_ptz_focus(pNDI_recv, abs_focus);
		}
	}

	{
		output->channels[0][0] = abs_pan;
		output->channels[1][0] = abs_tilt;
		output->channels[2][0] = abs_zoom;
		output->channels[3][0] = abs_focus;
	}
	
}

int32_t
NDI_CameraControl_CHOP::getNumInfoCHOPChans(void* reserved1)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send one channel.
	return 1;
}

void
NDI_CameraControl_CHOP::getInfoCHOPChan(int32_t index,
	OP_InfoCHOPChan* chan,
	void* reserved1)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name->setString("offset");
		chan->value = (float)myOffset;
	}
}

bool
NDI_CameraControl_CHOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
NDI_CameraControl_CHOP::getInfoDATEntries(int32_t index,
	int32_t nEntries,
	OP_InfoDATEntries* entries,
	void* reserved1)
{
	char tempBuffer[4096];

	if (index == 0)
	{
		// Set the value for the first column
		entries->values[0]->setString("executeCount");

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%d", myExecuteCount);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%d", myExecuteCount);
#endif
		entries->values[1]->setString(tempBuffer);
	}

	if (index == 1)
	{
		// Set the value for the first column
		entries->values[0]->setString("offset");

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%g", myOffset);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
#endif
		entries->values[1]->setString(tempBuffer);
	}
}


// GUI Elements
void
NDI_CameraControl_CHOP::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
	// Currently useless
	{
		OP_StringParameter sp;

		sp.name = "Cameraip";
		sp.label = "Camera IP";

		sp.defaultValue = "127.0.0.1";

		OP_ParAppendResult res = manager->appendString(sp);
		assert(res == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter	np;

		np.name = "Abspan";
		np.label = "Absolute Pan";
		np.defaultValues[0] = 1.0;
		np.minSliders[0] = -1.0;
		np.maxSliders[0] = 1.0;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter np;

		np.name = "Abstilt";
		np.label = "Absolute Tilt";

		np.defaultValues[0] = 0.;
		np.minSliders[0] = -1.;
		np.maxSliders[0] = 1.;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter np;

		np.name = "Abszoom";
		np.label = "Absolute Zoom";

		np.defaultValues[0] = 0.;
		np.minSliders[0] = 0.;
		np.maxSliders[0] = 1.;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter np;

		np.name = "Absfocus";
		np.label = "Absolute Focus";

		np.defaultValues[0] = 0.;
		np.minSliders[0] = 0.;
		np.maxSliders[0] = 1.;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}
}

void
NDI_CameraControl_CHOP::pulsePressed(const char* name, void* reserved1)
{
	if (!strcmp(name, "Reset"))
	{
		myOffset = 0.0;
	}
}

