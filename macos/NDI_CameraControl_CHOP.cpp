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
 * // NDI PTZ Camera controller v0.1.2 \\
 *    Made by Kostiantyn Yerokhin
 */

#include "NDI_CameraControl_CHOP.h"

extern "C"
{
DLLEXPORT

NDIlib_v3* pNDILib;
const NDIlib_source_t* p_sources;
NDIlib_find_instance_t pNDI_find;
NDIlib_recv_create_v3_t NDI_recv_create_desc;
NDIlib_recv_instance_t pNDI_recv;

void
FillCHOPPluginInfo(TD::CHOP_PluginInfo* info)
{
    // Always set this to CHOPCPlusPlusAPIVersion.
    info->apiVersion = TD::CHOPCPlusPlusAPIVersion;
    
    // The opType is the unique name for this CHOP. It must start with a
    // capital A-Z character, and all the following characters must lower case
    // or numbers (a-z, 0-9)
    info->customOPInfo.opType->setString("Ndicameracontroller");
    
    // The opLabel is the text that will show up in the OP Create Dialog
    info->customOPInfo.opLabel->setString("NDI Camera Controller");
    
    // Information about the author of this OP
    info->customOPInfo.authorName->setString("Kostiantyn Yerokhin");
    info->customOPInfo.authorEmail->setString("kostya29erohin@gmail.com");
    
    info->customOPInfo.minInputs = 0;
    info->customOPInfo.maxInputs = 0;
}

DLLEXPORT
TD::CHOP_CPlusPlusBase*
CreateCHOPInstance(const TD::OP_NodeInfo* info)
{
    // Return a new instance of your class every time this is called.
    // It will be called once per CHOP that is using the .dll
    return new NDI_CameraControl_CHOP(info);
}

DLLEXPORT
void
DestroyCHOPInstance(TD::CHOP_CPlusPlusBase* instance)
{
    // Delete the instance here, this will be called when
    // Touch is shutting down, when the CHOP using that instance is deleted, or
    // if the CHOP loads a different DLL
    delete (NDI_CameraControl_CHOP*)instance;
}

};

// Camera PTZ values will be initialized after first &::execute run
NDI_CameraControl_CHOP::NDI_CameraControl_CHOP(const TD::OP_NodeInfo* info) : myNodeInfo(info)
{
    myExecuteCount = 0;
    myOffset = 0.0;
    
    std::string ndi_path = "/usr/local/lib/libndi.dylib";
    
    
    void *hNDILib = ::dlopen(ndi_path.c_str(), RTLD_LOCAL | RTLD_LAZY);
    
    // The main NDI entry point for dynamic loading if we got the library
    NDIlib_v3* (*NDIlib_v3_load)(void) = NULL;
    if (hNDILib)
        *((void**)&NDIlib_v3_load) = ::dlsym(hNDILib, "NDIlib_v3_load");
    
    if (!NDIlib_v3_load)
    {
        printf("Please re-install the NewTek NDI Runtimes to use this application.");
        return;
    }
    
    // Lets get all of the DLL entry points
    pNDILib = NDIlib_v3_load();
    
    // We can now run as usual
    if (!pNDILib->initialize())
    {    // Cannot run NDI. Most likely because the CPU is not sufficient (see SDK documentation).
        // you can check this directly with a call to NDIlib_is_supported_CPU()
        printf("Cannot run NDI\n");
        if (!pNDILib->NDIlib_is_supported_CPU) {
            printf("CPU is not supported\n");
        }
        
    } else {
        printf("NDI Initialization is succesfull\n");
    }
    
    const NDIlib_find_create_t NDI_find_create_desc = { true, NULL };

    pNDI_find = pNDILib->NDIlib_find_create_v2(&NDI_find_create_desc);
    
    memset(source_names, 0, sizeof(source_names));
    memset(source_ips, 0, sizeof(source_ips));
    
    if (!pNDILib->NDIlib_initialize()) {
        // Cannot run NDI. Most likely because the CPU is not sufficient (see SDK
        // documentation). you can check this directly with a call to
        // NDIlib_is_supported_CPU()
        printf("Cannot run NDI.\n");
    }
    UpdateSources();
}

NDI_CameraControl_CHOP::~NDI_CameraControl_CHOP()
{
    pNDILib->NDIlib_recv_destroy(pNDI_recv);
    pNDILib->NDIlib_find_destroy(pNDI_find);
    pNDILib->NDIlib_destroy();
}

void
NDI_CameraControl_CHOP::getGeneralInfo(TD::CHOP_GeneralInfo* ginfo, const TD::OP_Inputs* inputs, void* reserved1)
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
NDI_CameraControl_CHOP::getOutputInfo(TD::CHOP_OutputInfo* info, const TD::OP_Inputs* inputs, void* reserved1)
{
    {
        info->numChannels = 24;
        // Since we are outputting a timeslice, the system will dictate
        // the numSamples and startIndex of the CHOP data
        info->numSamples = 1;
        info->startIndex = 0;
        
        // For illustration we are going to output 120hz data
        info->sampleRate = 25;
        return true;
    }
}

void
NDI_CameraControl_CHOP::getChannelName(int32_t index, TD::OP_String* name, const TD::OP_Inputs* inputs, void* reserved1)
{
    switch (index) {
        case 0: {name->setString("abs_pan"); break; }
        case 1: {name->setString("abs_tilt"); break; }
        case 2: {name->setString("abs_zoom"); break; }
        case 3: {name->setString("abs_focus"); break; }
        case 4: {name->setString("speed_pan"); break; }
        case 5: {name->setString("speed_tilt"); break; }
        case 6: {name->setString("speed_zoom"); break; }
        case 7: {name->setString("speed_focus"); break; }
        case 8: {name->setString("gain"); break; }
        case 9: {name->setString("iris"); break; }
        case 10: {name->setString("shutter_speed"); break; }
        default: {name->setString("unknown_channel"); break; }
    }
}

void
NDI_CameraControl_CHOP::execute(TD::CHOP_Output* output,
                                const TD::OP_Inputs* inputs,
                                void* reserved)
{
    myExecuteCount++;
    
//    inputs->enablePar("Absolutevalues", 1);
    inputs->enablePar("Availablesources", 1);
    
    inputs->enablePar("Speedpan", 1);
    inputs->enablePar("Speedtilt", 1);
    inputs->enablePar("Speedzoom", 1);
    inputs->enablePar("Speedfocus", 1);
    
    inputs->enablePar("Abspan", 1);
    inputs->enablePar("Abstilt", 1);
    inputs->enablePar("Abszoom", 1);
    inputs->enablePar("Absfocus", 1);
    
    inputs->enablePar("Gain", 1);
    inputs->enablePar("Iris", 1);
    inputs->enablePar("Shutterspeed", 1);
    
    const char* selected_id = inputs->getParString("Availablesources");
    double abs_pan_new = inputs->getParDouble("Abspan");
    double abs_tilt_new = inputs->getParDouble("Abstilt");
    double abs_zoom_new = inputs->getParDouble("Abszoom");
    double abs_focus_new = inputs->getParDouble("Absfocus");
    
    double speed_pan_new = inputs->getParDouble("Speedpan");
    double speed_tilt_new = inputs->getParDouble("Speedtilt");
    double speed_zoom_new = inputs->getParDouble("Speedzoom");
    double speed_focus_new = inputs->getParDouble("Speedfocus");
    
    double gain_new = inputs->getParDouble("Gain");
    double iris_new = inputs->getParDouble("Iris");
    double shutter_speed_new = inputs->getParDouble("Shutterspeed");
    
//    int current_mode = inputs->getParInt("Absolutevalues");
    
    if ((char*)selected_id != selected_id_old) {
        selected_id_old = (char*)selected_id;
        this->ConnectByURL(selected_id);
        printf("Selected source changed\n");
        printf("Connecting to camera at %s\n", selected_id);
    }
    
    {
        // Speed Values
        {
            if (cam_data.speed_pan != speed_pan_new || cam_data.speed_tilt != speed_tilt_new) {
                cam_data.speed_pan = speed_pan_new;
                cam_data.speed_tilt = speed_tilt_new;
                pNDILib->NDIlib_recv_ptz_pan_tilt_speed(pNDI_recv, cam_data.speed_pan, cam_data.speed_tilt);
            }
            
            if (cam_data.speed_zoom != inputs->getParDouble("Speedzoom")) {
                cam_data.speed_zoom = speed_zoom_new;
                pNDILib->NDIlib_recv_ptz_zoom_speed(pNDI_recv, cam_data.speed_zoom);
            }
            
            if (cam_data.speed_focus != inputs->getParDouble("Speedfocus")) {
                cam_data.speed_focus = speed_focus_new;
                pNDILib->NDIlib_recv_ptz_focus_speed(pNDI_recv, cam_data.speed_focus);
            }
        }
        
        // Absolute values
        {
            if (cam_data.abs_pan != abs_pan_new || cam_data.abs_tilt != abs_tilt_new) {
                cam_data.abs_pan = abs_pan_new;
                cam_data.abs_tilt = abs_tilt_new;
                pNDILib->NDIlib_recv_ptz_pan_tilt(pNDI_recv, cam_data.abs_pan, cam_data.abs_tilt);
            }
            if (cam_data.abs_zoom != inputs->getParDouble("Abszoom")) {
                cam_data.abs_zoom = abs_zoom_new;
                pNDILib->NDIlib_recv_ptz_zoom(pNDI_recv, cam_data.abs_zoom);
            }
            if (cam_data.abs_focus != inputs->getParDouble("Absfocus")) {
                cam_data.abs_focus = abs_focus_new;
                pNDILib->NDIlib_recv_ptz_focus(pNDI_recv, cam_data.abs_focus);
            }
        }
        
        // Exposure values
        {
            if (cam_data.iris != iris_new || cam_data.gain != gain_new || cam_data.shutter_speed != shutter_speed_new) {
                cam_data.gain = gain_new;
                cam_data.iris = iris_new;
                cam_data.shutter_speed = shutter_speed_new;
                pNDILib->NDIlib_recv_ptz_exposure_manual_v2(pNDI_recv, cam_data.iris, cam_data.gain, cam_data.shutter_speed);
            }
        }
        
        output->channels[0][0] = cam_data.abs_pan;
        output->channels[1][0] = cam_data.abs_tilt;
        output->channels[2][0] = cam_data.abs_zoom;
        output->channels[3][0] = cam_data.abs_focus;
        output->channels[4][0] = cam_data.speed_pan;
        output->channels[5][0] = cam_data.speed_tilt;
        output->channels[6][0] = cam_data.speed_zoom;
        output->channels[7][0] = cam_data.speed_focus;
        output->channels[8][0] = cam_data.gain;
        output->channels[9][0] = cam_data.iris;
        output->channels[10][0] = cam_data.shutter_speed;
        
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
                                        TD::OP_InfoCHOPChan* chan,
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
NDI_CameraControl_CHOP::getInfoDATSize(TD::OP_InfoDATSize* infoSize, void* reserved1)
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
                                          TD::OP_InfoDATEntries* entries,
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
NDI_CameraControl_CHOP::setupParameters(TD::OP_ParameterManager* manager, void* reserved1)
{
    {
        TD::OP_StringParameter sp;

        sp.name = "Availablesources";
        sp.label = "Avalable Sources";

        sp.defaultValue = "None";

        int num_of_sources = 0;
        for (int i = 0; source_names[i] != 0; i++) {
            num_of_sources++;
        }

        sp.page = "Camera Controls";

        TD::OP_ParAppendResult res = manager->appendStringMenu(sp, num_of_sources, source_ips, source_names);
        assert(res == TD::OP_ParAppendResult::Success);
    }
    // ABSOLUTE VALUES MODE
    {
        {
            TD::OP_NumericParameter    np;
            
            np.name = "Abspan";
            np.label = "Absolute Pan";
            
            np.defaultValues[0] = 0.0;
            np.minSliders[0] = -1.0;
            np.maxSliders[0] = 1.0;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Abstilt";
            np.label = "Absolute Tilt";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = -1.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Abszoom";
            np.label = "Absolute Zoom";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = 0.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Absfocus";
            np.label = "Absolute Focus";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = 0.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
    }
    
    // SPEED VALUES MODE
    {
        {
            TD::OP_NumericParameter    np;
            
            np.name = "Speedpan";
            np.label = "Pan Speed";
            
            np.defaultValues[0] = 0.0;
            np.minSliders[0] = -1.0;
            np.maxSliders[0] = 1.0;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Speedtilt";
            np.label = "Tilt Speed";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = -1.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Speedzoom";
            np.label = "Zoom Speed";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = -1.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Speedfocus";
            np.label = "Focus Speed";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = -1.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        // EXPOSURE SETTINGS
        {
            TD::OP_NumericParameter np;
            
            np.name = "Gain";
            np.label = "Gain";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = 0.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Iris";
            np.label = "Iris";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = 0.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
        
        {
            TD::OP_NumericParameter np;
            
            np.name = "Shutterspeed";
            np.label = "Shutter Speed";
            
            np.defaultValues[0] = 0.;
            np.minSliders[0] = 0.;
            np.maxSliders[0] = 1.;
            
            np.page = "Camera Controls";
            
            TD::OP_ParAppendResult res = manager->appendFloat(np);
            assert(res == TD::OP_ParAppendResult::Success);
        }
    }
    
    // Update sources
    // Disabled since it's impossible to update GUI values without CHOP Re-Init
    //{
    //    OP_NumericParameter np;
    
    //    np.name = "Updatesources";
    //    np.label = "Update Sources";
    
    //    np.defaultValues[0] = 1;
    
    //    np.page = "Camera Controls";
    
    //    OP_ParAppendResult res = manager->appendPulse(np);
    //    assert(res == OP_ParAppendResult::Success);
    //}
    
    // Absolute values <-> Value Change Speed MODE switch
    /*
     {
     OP_NumericParameter np;
     
     np.name = "Absolutevalues";
     np.label = "Absolute Values";
     
     np.defaultValues[0] = 1;
     
     np.page = "Camera Controls";
     
     OP_ParAppendResult res = manager->appendToggle(np);
     assert(res == OP_ParAppendResult::Success);
     }
     */
}

void
NDI_CameraControl_CHOP::pulsePressed(const char* name, void* reserved1)
{
    if (!strcmp(name, "Updatesources")) {
        UpdateSources();
    }
}

void
NDI_CameraControl_CHOP::UpdateSources() {
    if (!pNDILib->NDIlib_find_wait_for_sources(pNDI_find, 1000 /* milliseconds */)) {
        printf("No change to the sources found.\n");
    }
    
    // Get the updated list of sources
    uint32_t no_sources = 0;
    p_sources = pNDILib->NDIlib_find_get_current_sources(pNDI_find, &no_sources);
    
    // Display all the sources.
    printf("Network sources (%u found).\n", no_sources);
    for (uint32_t i = 0; i < no_sources; i++) {
        
        source_ips[i] = p_sources[i].p_url_address;
        source_names[i] = p_sources[i].p_ndi_name;
        
        printf("%u. %s\t\t", i + 1, p_sources[i].p_ndi_name);
        printf("%s\n", p_sources[i].p_url_address);
        
    }
}

void NDI_CameraControl_CHOP::ConnectByURL(const char* camera_url) {
    NDIlib_source_t ndi_source = { "Custom source", camera_url };
    
    NDI_recv_create_desc.source_to_connect_to = ndi_source;
    NDI_recv_create_desc.p_ndi_recv_name = "TD->NDI Camera Controller";
    pNDI_recv = pNDILib->NDIlib_recv_create_v3(&NDI_recv_create_desc);
    if (!pNDI_recv) {
        printf("Error connecting to NDI source\n");
    }
}

void NDI_CameraControl_CHOP::ConnectByID(int id) {
    
    UpdateSources();
    
    NDI_recv_create_desc.source_to_connect_to = p_sources[id];
    NDI_recv_create_desc.p_ndi_recv_name = "TD->NDI Camera Controller made by Kostiantyn Yerokhin";
    pNDI_recv = pNDILib->NDIlib_recv_create_v3(&NDI_recv_create_desc);
    if (!pNDI_recv) {
        printf("Error connecting to NDI source\n");
    }
}
