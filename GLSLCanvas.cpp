#include "GLSLCanvas.h"

#include "GLBase.h"
#include "LoadShaders.h"
#include "SystemUtil.h"

#include <iostream>


namespace {
	
	// common GL resources
	CGLContextObj		renderContext = 0;
	NSOpenGLContext		*context = 0;
	std::string			resourcePath;
	
	GLuint swizzleOutputProgram = 0;
    GLuint swizzleInputProgram  = 0;
	
	GLuint vao = 0;
	GLuint quad = 0;
	
	
	GLuint InitProgram(std::string fragmentFilePath) {
		
		GLuint program = LoadShaders((resourcePath + "vertex-shader.vert").c_str(),
									 fragmentFilePath.c_str());
		return program;
	}
	
	void ToggleFlag(A_long &target, A_long flag, A_Boolean value) {
		if (value) {
			target |= flag;
		} else {
			target &= ~flag;
		}
	}
	
	void CreateQuad() {
		
		// make and bind the VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		
		// make and bind the VBO
		GLfloat positions[] = {
			-1, -1, 0,
			+1, -1, 0,
			-1, +1, 0,
			+1, +1, 0
		};
		
		glGenBuffers(1, &quad);
		glBindBuffer(GL_ARRAY_BUFFER, quad);
		glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
		
		glBindVertexArray(0);
	}
	
    void RenderQuad(EffectRenderData *renderData) {
        
        glBindVertexArray(vao);
		glEnableVertexAttribArray(glGetAttribLocation(renderData->program, "position"));
		glBindBuffer(GL_ARRAY_BUFFER, quad);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
	}
	
	
	void SetPluginContext() {
		if (context) {
			[context makeCurrentContext];
		}
		AESDK_OpenGL::makeCurrentFlush(renderContext);
	}
	
	void MakeReadyToRender(EffectRenderData *renderData, GLuint texture) {
		glBindFramebuffer(GL_FRAMEBUFFER, renderData->frameBuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}
    
    GLuint AllocateTexture(u_int16 width,
                           u_int16 height,
                           GLint format = GL_RGBA8,
                           GLint filter = GL_NEAREST) {
        
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        
        // Give an empty image to OpenGL ( the last "0" )
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        
        // Poor filtering. Needed !
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        
        return texture;
    }
	
	void BindTextureToTarget(GLuint program, GLuint texture, std::string targetName) {
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(glGetUniformLocation(program, targetName.c_str()), 0);
	}
	
	void RenderGL(EffectRenderData *renderData, GLfloat width, GLfloat height, GLfloat time, A_FloatPoint mouse, GLuint tex0) {
		
		glClear(GL_COLOR_BUFFER_BIT);
		
		glBindTexture(GL_TEXTURE_2D, tex0);
		
		glUseProgram(renderData->program);
		
		glUniform2f(glGetUniformLocation(renderData->program, "u_resolution"),	width, height);
		glUniform1f(glGetUniformLocation(renderData->program, "u_time"),		time);
		glUniform2f(glGetUniformLocation(renderData->program, "u_mouse"),		mouse.x, mouse.y);
		BindTextureToTarget(renderData->program, tex0, std::string("u_tex0"));

		RenderQuad(renderData);
		
		glUseProgram(0);
	}
	
	void SwizzleGL(EffectRenderData *renderData, GLfloat width, GLfloat height) {
		
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(swizzleOutputProgram);
		
		glUniform2f(glGetUniformLocation(swizzleOutputProgram, "u_resolution"),	width, height);
		BindTextureToTarget(swizzleOutputProgram, renderData->beforeSwizzleTexture, std::string("videoTexture"));
    
		RenderQuad(renderData);
		
		glUseProgram(0);
		glFlush();
	}
	
	GLuint UploadTexture(AEGP_SuiteHandler& suites,					// >>
                         EffectRenderData* renderData,
						//PF_PixelFormat			format,				// >>
						PF_EffectWorld			*input_worldP,		// >>
						PF_InData				*in_data)			// >>
						//size_t& pixSizeOut,							// <<
						//GLenum& glFmtOut,							// <<
						//float& multiplier16bitOut)					// <<
	{
		// - upload to texture memory
        
        GLuint rawTexture = AllocateTexture(input_worldP->width, input_worldP->height);

		//multiplier16bitOut = 1.0f;
		//switch (format) {
			/*
			case PF_PixelFormat_ARGB128:
			{
				glFmtOut = GL_FLOAT;
				pixSizeOut = sizeof(PF_PixelFloat);
				
				std::auto_ptr<PF_PixelFloat> bufferFloat(new PF_PixelFloat[input_worldP->width * input_worldP->height]);
				CopyPixelFloat_t refcon = { bufferFloat.get(), input_worldP };
				
				CHECK(suites.IterateFloatSuite1()->iterate(in_data,
														   0,
														   input_worldP->height,
														   input_worldP,
														   nullptr,
														   reinterpret_cast<void*>(&refcon),
														   CopyPixelFloatIn,
														   output_worldP));
				
				glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->width);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, GL_FLOAT, bufferFloat.get());
				break;
			}
				
			case PF_PixelFormat_ARGB64:
			{
				glFmtOut = GL_UNSIGNED_SHORT;
				pixSizeOut = sizeof(PF_Pixel16);
				multiplier16bitOut = 65535.0f / 32768.0f;
				
				glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->rowbytes / sizeof(PF_Pixel16));
				PF_Pixel16 *pixelDataStart = NULL;
				PF_GET_PIXEL_DATA16(input_worldP, NULL, &pixelDataStart);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, GL_UNSIGNED_SHORT, pixelDataStart);
				break;
			}
			 */
				
			//case PF_PixelFormat_ARGB32:
			//{
				//glFmtOut = GL_UNSIGNED_BYTE;
				//pixSizeOut = sizeof(PF_Pixel8);
				
				glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->rowbytes / sizeof(PF_Pixel8));
				PF_Pixel8 *pixelDataStart = NULL;
				PF_GET_PIXEL_DATA8(input_worldP, NULL, &pixelDataStart);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, GL_UNSIGNED_BYTE, pixelDataStart);
				//break;
			//}
		//}
		
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		
		//unbind all textures
		glBindTexture(GL_TEXTURE_2D, 0);
        
        // swizzle channels
        GLuint texture = AllocateTexture(renderData->width, renderData->height);
        MakeReadyToRender(renderData, texture);
        
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(swizzleInputProgram);
        
        glUniform2f(glGetUniformLocation(swizzleOutputProgram, "u_resolution"), renderData->width, renderData->height);
        BindTextureToTarget(swizzleOutputProgram, renderData->beforeSwizzleTexture, std::string("videoTexture"));
        
        RenderQuad(renderData);
        
        glUseProgram(0);
        glDeleteTextures(1, &rawTexture);
        
		return texture;
	}
	
	std::string GetResourcesPath(PF_InData *in_data) {
		//initialize and compile the shader objects
		A_UTF16Char pluginFolderPath[AEFX_MAX_PATH];
		PF_GET_PLATFORM_DATA(PF_PlatData_EXE_FILE_PATH_W, &pluginFolderPath);
		
		NSUInteger length = 0;
		A_UTF16Char* tmp = pluginFolderPath;
		while (*tmp++ != 0) {
			++length;
		}
		NSString* newStr = [[NSString alloc] initWithCharacters:pluginFolderPath length : length];
		std::string path([newStr UTF8String]);
		path += "/Contents/Resources/";
		
		return path;
	}
	
	void SetupRenderData(EffectRenderData* renderData, u_int16 width, u_int16 height) {
		
		bool sizeChanged = renderData->width != width || renderData->height != height;
		
		renderData->width = width;
		renderData->height = height;
		
		if (sizeChanged) {
			
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			
			// release framebuffer resources
			if (renderData->frameBuffer) {
				glDeleteFramebuffers(1, &renderData->frameBuffer);
				renderData->frameBuffer = 0;
			}
			
			if (renderData->beforeSwizzleTexture) {
				glDeleteTextures(1, &renderData->beforeSwizzleTexture);
				renderData->beforeSwizzleTexture = 0;
			}
			
			if (renderData->outputFrameTexture) {
				glDeleteTextures(1, &renderData->outputFrameTexture);
				renderData->outputFrameTexture = 0;
			}
		}
		
		// Create a frame-buffer object and bind it...
		if (renderData->frameBuffer == 0) {
			glGenFramebuffers(1, &renderData->frameBuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, renderData->frameBuffer);
		}
		
		if (renderData->beforeSwizzleTexture == 0) {
			std::cout << "beforeSwizzleTexture Reallocated" << std::endl;
            renderData->beforeSwizzleTexture = AllocateTexture(width, height);
		}
		
		if (renderData->outputFrameTexture == 0) {
            std::cout << "outputFrameTexture Reallocated" << std::endl;
            renderData->outputFrameTexture = AllocateTexture(width, height);
		}
		
		if (renderData->program == 0) {
			
			std::string fragPath = strlen(renderData->fragPath) == 0
				? resourcePath + "fragment-shader.frag"
				: std::string(renderData->fragPath);
			
			std::cout << "Recompile shader programs fragPath=" << fragPath  << std::endl;

			renderData->program = InitProgram(fragPath);
		}
		
	}
};

//---------------------------------------------------------------------------
static PF_Err
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	EffectRenderData *renderData = reinterpret_cast<EffectRenderData*>(*(in_data->sequence_data));
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s\r\rShader Location:%s",
											STR(StrID_Name), 
											MAJOR_VERSION, 
											MINOR_VERSION,
											STR(StrID_Description),
											strlen(renderData->fragPath) == 0 ? "<Not Loaded>" : renderData->fragPath);
	return PF_Err_NONE;
}

//---------------------------------------------------------------------------
static PF_Err
PopDialog (
   PF_InData		*in_data,
   PF_OutData		*out_data,
   PF_ParamDef		*params[],
   PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;
	
	std::vector<std::string> fileTypes;
	
	fileTypes.push_back("frag");
	fileTypes.push_back("glsl");
	fileTypes.push_back("fs");
	
	std::string path = AESDK_SystemUtil::openFileDialog(fileTypes);
	
	if (!path.empty()) {
		std::cout << "fragPath=" << path << std::endl;
		InitProgram(path);
		
		EffectRenderData *renderData = *(EffectRenderData**)out_data->sequence_data;
		STRNCPY(renderData->fragPath, path.c_str(), FRAGPATH_MAX_LEN);
		
		glDeleteProgram(renderData->program);
		renderData->program = 0;
		
	} else {
		std::cout << "fragPath=: Not Changed" << std::endl;
	}
	
	out_data->out_flags |= PF_OutFlag_SEND_DO_DIALOG;
	
	return err;
}

//---------------------------------------------------------------------------
static PF_Err
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler		suites(in_data->pica_basicP);
	
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags =  PF_OutFlag_NON_PARAM_VARY | PF_OutFlag_I_DO_DIALOG;
	out_data->out_flags2 = PF_OutFlag2_NONE;
	
	PF_Err err = PF_Err_NONE;
	
	try {
		resourcePath = GetResourcesPath(in_data);
		
		
		// always restore back AE's own OGL context
		AESDK_OpenGL::SaveRestoreOGLContext oSavedContext;
		
		// setup common resources
		context = AESDK_OpenGL::createNSContext(nullptr, renderContext);
		SetPluginContext();
		
		CreateQuad();
		
        swizzleInputProgram = InitProgram(resourcePath + "swizzle-input.frag");
        swizzleOutputProgram = InitProgram(resourcePath + "swizzle-output.frag");
		
		std::cout << std::endl
			<< "OpenGL Version:			" << glGetString(GL_VERSION) << std::endl
			<< "OpenGL Vendor:			" << glGetString(GL_VENDOR) << std::endl
			<< "OpenGL Renderer:		" << glGetString(GL_RENDERER) << std::endl
			<< "OpenGL GLSL Versions:	" << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
		
		// setup global data
		EffectGlobalData	*globalData = NULL;
		PF_Handle			globalDataH = suites.HandleSuite1()->host_new_handle(sizeof(EffectGlobalData));
		if (globalDataH) {
			globalData = reinterpret_cast<EffectGlobalData*>(suites.HandleSuite1()->host_lock_handle(globalDataH));
			
			if (globalData) {
				globalData->initialized = TRUE;
			}
			if (in_data->appl_id != 'PrMr') {
				// This is only needed for the AEGP suites, which Premiere Pro doesn't support
				ERR(suites.UtilitySuite3()->AEGP_RegisterWithAEGP(NULL, STR(StrID_Name), &globalData->ID));
			}
			if (!err) {
				out_data->global_data = globalDataH;
			}
			suites.HandleSuite1()->host_unlock_handle(globalDataH);
		} else {
			err = PF_Err_INTERNAL_STRUCT_DAMAGED;
		}
		
		
		
	} catch(PF_Err& thrown_err) {
		err = thrown_err;
	} catch (...) {
		err = PF_Err_OUT_OF_MEMORY;
	}
	
	return err;
}

//---------------------------------------------------------------------------
static PF_Err
GlobalSetdown (
			   PF_InData		*in_data,
			   PF_OutData		*out_data,
			   PF_ParamDef		*params[],
			   PF_LayerDef		*output )
{
	PF_Err			err			=	PF_Err_NONE;
	
	try
	{
		// always restore back AE's own OGL context
		AESDK_OpenGL::SaveRestoreOGLContext oSavedContext;
		
		if (vao) {
			glDeleteBuffers(1, &quad);
			glDeleteVertexArrays(1, &vao);
		}
		
		if (in_data->sequence_data) {
			PF_DISPOSE_HANDLE(in_data->sequence_data);
			out_data->sequence_data = NULL;
		}
		
	} catch(PF_Err& thrown_err) {
		err = thrown_err;
	} catch (...) {
		err = PF_Err_OUT_OF_MEMORY;
	}
	
	return err;
}

//---------------------------------------------------------------------------
static PF_Err
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	PF_ParamDef		def;
	
	
	
	// Customize the name of the options button
	// Premiere Pro/Elements does not support this suite
	if (in_data->appl_id != 'PrMr') {
		AEFX_SuiteScoper<PF_EffectUISuite1> effect_ui_suiteP = AEFX_SuiteScoper<PF_EffectUISuite1>(in_data,
																								   kPFEffectUISuite,
																								   kPFEffectUISuiteVersion1,
																								   out_data);
		
		ERR(effect_ui_suiteP->PF_SetOptionsButtonName(in_data->effect_ref, "Load Shader.."));
	}
	
	// Add parameters
	
	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(STR(StrID_Use_Layer_Time_Param_Name),	// NAME_A
					"",										// NAME_B
					TRUE,									// Default
					PF_ParamFlag_SUPERVISE,					// Flags
					FILTER_USE_LAYER_TIME_DISK_ID);			// ID
	
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Time_Param_Name),
						 -100000,					// VALID_MIN
						 100000,					// VALID_MAX
						 0,							// SLIDER_MIN
						 10,						// SLIDER_MAX
						 0,							// Default
						 4,							// Precision
						 PF_ValueDisplayFlag_NONE,	// Display
						 0,							// Flags
						 FILTER_TIME_DISK_ID);		// ID
	
	
	// TODO: set default mouse position to center of layer
	AEFX_CLR_STRUCT(def);
	PF_ADD_POINT(STR(StrID_Mouse_Param_Name),
				 (A_long)(in_data->width / 2.0f),
				 (A_long)(in_data->height / 2.0f),
				 RESTRICT_BOUNDS,
				 MOUSE_DISK_ID);
	
	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(STR(StrID_Show_Error_Name),	// PARAM_BANE
				  "Show",						// BUTTON_NAME
				  PF_PUI_NONE,					// PUI_FLAGS
				  PF_ParamFlag_SUPERVISE,		// PARAM_FLAGS
				  FILTER_SHOW_ERROR_DISK_ID);	// ID
	
//	AEFX_CLR_STRUCT(def);
//	PF_ADD_LAYER("u_tex0", PF_LayerDefault_MYSELF, FILTER_TEX0_DISK_ID);
//	
//	AEFX_CLR_STRUCT(def);
//	PF_ADD_LAYER("u_tex1", PF_LayerDefault_MYSELF, FILTER_TEX1_DISK_ID);
//	
//	AEFX_CLR_STRUCT(def);
//	PF_ADD_LAYER("u_tex2", PF_LayerDefault_MYSELF, FILTER_TEX2_DISK_ID);
//	
//	AEFX_CLR_STRUCT(def);
//	PF_ADD_LAYER("u_tex3", PF_LayerDefault_MYSELF, FILTER_TEX3_DISK_ID);
//	
//	AEFX_CLR_STRUCT(def);
//	PF_ADD_LAYER("u_tex4", PF_LayerDefault_MYSELF, FILTER_TEX4_DISK_ID);
	
	out_data->num_params = FILTER_NUM_PARAMS;

	return err;
}

//---------------------------------------------------------------------------
static PF_Err
SequenceSetup (
			   PF_InData		*in_data,
			   PF_OutData		*out_data,
			   PF_ParamDef		*params[],
			   PF_LayerDef		*output )
{
	PF_Err				err = PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	std::cout << "SsequenceSetup Called" << std::endl;
	
	// Create sequence data
	PF_Handle			effectRenderDataH =	suites.HandleSuite1()->host_new_handle(sizeof(EffectRenderData));
	
	if (effectRenderDataH){
		EffectRenderData	*renderData = reinterpret_cast<EffectRenderData*>(suites.HandleSuite1()->host_lock_handle(effectRenderDataH));
		
		if (renderData){
			AEFX_CLR_STRUCT(*renderData);
			
			renderData->flat = FALSE;
			STRNCPY(renderData->fragPath, "", FRAGPATH_MAX_LEN);
			
			out_data->sequence_data = effectRenderDataH;
			
			suites.HandleSuite1()->host_unlock_handle(effectRenderDataH);
		}
	} else {	// whoa, we couldn't allocate sequence data; bail!
		err = PF_Err_OUT_OF_MEMORY;
	}
	
	if (err) {
		PF_SPRINTF(out_data->return_msg, "SequenceSetup failure in HistoGrid Effect");
		out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
	}
	
	return err;
}

//---------------------------------------------------------------------------
static PF_Err
SequenceSetdown (
	 PF_InData		*in_data,
	 PF_OutData		*out_data,
	 PF_ParamDef	*params[],
	 PF_LayerDef	*output )
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	std::cout << "SequenceSetdown Called" << std::endl;
	
	// Flat or unflat, get rid of it
	if (in_data->sequence_data){
		suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
	}
	return err;
}

//---------------------------------------------------------------------------
// EffectRenderData -> FlatSeqData
static PF_Err
SequenceFlatten(
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	std::cout << "SequenceFlatten Called" << std::endl;
	
	// Make a flat copy of whatever is in the unflat seq data handed to us.
	if (in_data->sequence_data) {
		EffectRenderData* renderData = reinterpret_cast<EffectRenderData*>(DH(in_data->sequence_data));
		
		if (renderData) {
			PF_Handle flatSeqDataH = suites.HandleSuite1()->host_new_handle(sizeof(EffectRenderData));
			
			if (flatSeqDataH){
				EffectRenderData*	flatSeqData = reinterpret_cast<EffectRenderData*>(suites.HandleSuite1()->host_lock_handle(flatSeqDataH));
				
				if (flatSeqData){
					AEFX_CLR_STRUCT(*flatSeqData);
					
					flatSeqData->flat = TRUE;
					STRNCPY(flatSeqData->fragPath, renderData->fragPath, FRAGPATH_MAX_LEN);
					std::cout << "fragPath:" << flatSeqData->fragPath << std::endl;
					
					// In SequenceSetdown we toss out the unflat data
					//delete renderData->fragPath;
					suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
					
					out_data->sequence_data = flatSeqDataH;
					suites.HandleSuite1()->host_unlock_handle(flatSeqDataH);
				} else {
					std::cout << "flatSeqDataH nooooo" << std::cout;
				}
				
			} else {
				err = PF_Err_INTERNAL_STRUCT_DAMAGED;
			}
		}
	} else {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

//---------------------------------------------------------------------------
static PF_Err
SequenceResetup (
				 PF_InData		*in_data,
				 PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	// We got here because we're either opening a project w/saved (flat) sequence data,
	// or we've just been asked to flatten our sequence data (for a save) and now
	// we're blowing it back up.
	
	std::cout << "SequenceResetup Called" <<  std::endl;
	
	//std::cout << "sizeof EffectRenderData:" << sizeof(EffectRenderData) << std::endl;
	//std::cout << "sizeof FlatSeqData:" << sizeof(FlatSeqData) << std::endl;
	
	if (in_data->sequence_data){
		EffectRenderData*	flatSeqDataP = reinterpret_cast<EffectRenderData*>(*(in_data->sequence_data));
		
		if (flatSeqDataP){
			PF_Handle renderDataH = suites.HandleSuite1()->host_new_handle(sizeof(EffectRenderData));
			
			if (renderDataH){
				EffectRenderData* renderData = reinterpret_cast<EffectRenderData*>(suites.HandleSuite1()->host_lock_handle(renderDataH));
				
				if (renderData){
					AEFX_CLR_STRUCT(*renderData);
					
					renderData->flat = FALSE;
					STRNCPY(renderData->fragPath, flatSeqDataP->fragPath, FRAGPATH_MAX_LEN);
					std::cout << "fragPath:" << renderData->fragPath << std::endl;
					
					suites.HandleSuite1()->host_unlock_handle(renderDataH);
				}
			}
		}
	}
	return err;
}

//---------------------------------------------------------------------------
static PF_Err
Render(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err				err			= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	EffectRenderData	*renderData = reinterpret_cast<EffectRenderData*>(*(in_data->sequence_data));
	
	std::cout << "Render Called flat=" << (renderData->flat ? "TRUE" : "FALSE");
	//std::cout << " fragPath=" << renderData->fragPath;

    try {
        u_int16 width = in_data->width, height = in_data->height;
        
		// always restore back AE's own OGL context
		AESDK_OpenGL::SaveRestoreOGLContext oSavedContext;
        SetPluginContext();
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		
		// upload the input world to a texture
		GLuint inputFrameTexture = UploadTexture(suites, renderData, &params[FILTER_INPUT]->u.ld, in_data);
		
		SetupRenderData(renderData, width, height);
		
		// RenderGL
		GLfloat time;
		if (params[FILTER_USE_LAYER_TIME_DISK_ID]->u.bd.value) {
			time = (GLfloat)in_data->current_time / (GLfloat)in_data->time_scale;
		} else {
			time = (GLfloat)params[FILTER_TIME_DISK_ID]->u.fs_d.value;
		}
		A_FloatPoint	mouse = {
			FIX_2_FLOAT(params[FILTER_MOUSE]->u.td.x_value),
			height - FIX_2_FLOAT(params[FILTER_MOUSE]->u.td.y_value)
		};
		MakeReadyToRender(renderData, renderData->beforeSwizzleTexture);
		RenderGL(renderData, width, height, time, mouse, inputFrameTexture);
		
		// Swizzle
		MakeReadyToRender(renderData, renderData->outputFrameTexture);
		SwizzleGL(renderData, width, height);
		
		// DownlodTexture
		size_t pixSize = sizeof(PF_Pixel8);
		
		PF_Handle bufferH = NULL;
		bufferH = suites.HandleSuite1()->host_new_handle(width * height * pixSize);
		void *bufferP = suites.HandleSuite1()->host_lock_handle(bufferH);
		
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bufferP);
		
		PF_Pixel8 *pixels = reinterpret_cast<PF_Pixel8*>(bufferP);
		
		for (int y = 0; y < output->height; ++y) {
			PF_Pixel8 *pixelDataStart = NULL;
			PF_GET_PIXEL_DATA8(output, NULL, &pixelDataStart);
			::memcpy(pixelDataStart + (y * output->rowbytes / pixSize),
					 pixels + y * width,
					 width * pixSize);
		}

		suites.HandleSuite1()->host_unlock_handle(bufferH);
		suites.HandleSuite1()->host_dispose_handle(bufferH);
		
		// end of DownloadTexture
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDeleteTextures(1, &inputFrameTexture);
		
	} catch (PF_Err& thrown_err) {
		err = thrown_err;
	} catch (...) {
		err = PF_Err_OUT_OF_MEMORY;
	}
	
	std::cout << std::endl;

	return err;
}

//---------------------------------------------------------------------------
static PF_Err
MakeParamCopy(
			  PF_ParamDef *actual[],	/* >> */
			  PF_ParamDef copy[])		/* << */
{
	for (A_short iS = FILTER_INPUT; iS < FILTER_NUM_PARAMS; ++iS)	{
		AEFX_CLR_STRUCT(copy[iS]);	// clean params are important!
		copy[iS] = *actual[iS];
	}
	
	return PF_Err_NONE;
	
}

//---------------------------------------------------------------------------
static PF_Err
UserChangedParam(
				 PF_InData						*in_data,
				 PF_OutData						*out_data,
				 PF_ParamDef					*params[],
				 const PF_UserChangedParamExtra	*which_hitP)
{
	PF_Err err = PF_Err_NONE;
	
	switch (which_hitP->param_index) {
			
		case FILTER_SHOW_ERROR_DISK_ID:
			PF_STRCPY(out_data->return_msg,
					  "Test");
			
			if (in_data->appl_id != 'PrMr') {
				out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			}
			break;
	}
	
	return err;
}

//---------------------------------------------------------------------------
static PF_Err
UpdateParameterUI(
				  PF_InData			*in_data,
				  PF_OutData			*out_data,
				  PF_ParamDef			*params[],
				  PF_LayerDef			*outputP)
{
	PF_Err				err			= PF_Err_NONE,
						err2		= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	EffectGlobalData	*globalData	= reinterpret_cast<EffectGlobalData*>(DH(out_data->global_data));
	AEGP_EffectRefH		effectH		= NULL;
	AEGP_StreamRefH		timeStreamH = NULL;
	
	
	std::cout << "UpdateParameterUI Called" << std::endl;
	
	//	Before we can change the enabled/disabled state of parameters,
	//	we need to make a copy (remember, parts of those passed into us
	//	are read-only).
	PF_ParamDef	paramsCopy[FILTER_NUM_PARAMS];
	ERR(MakeParamCopy(params, paramsCopy));
	
	if (!err) {
		
		A_Boolean useLayerTime = params[FILTER_USE_LAYER_TIME_DISK_ID]->u.bd.value;
		
		/*
		ToggleFlag(paramsCopy[FILTER_TIME_DISK_ID].ui_flags, PF_PUI_DISABLED, useLayerTime);
		
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
														FILTER_TIME_DISK_ID,
														&paramsCopy[FILTER_TIME_DISK_ID]));
		*/
		
		// Changing visibility of params in AE is handled through stream suites
		ERR(suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(globalData->ID, in_data->effect_ref, &effectH));
		
		ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(globalData->ID, effectH, FILTER_TIME_DISK_ID, &timeStreamH));
		
		// Toggle visibility of parameters
		ERR(suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(timeStreamH, AEGP_DynStreamFlag_HIDDEN, FALSE, useLayerTime));
		
		if (effectH) {
			ERR2(suites.EffectSuite2()->AEGP_DisposeEffect(effectH));
		}
		if (timeStreamH) {
			ERR2(suites.StreamSuite2()->AEGP_DisposeStream(timeStreamH));
		}

		if (!err){
			out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		}
		
	}
	
	return err;
}


DllExport	
PF_Err 
EntryPointFunc (
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:

				err = About(in_data,
							out_data,
							params,
							output);
				break;
			
			case PF_Cmd_DO_DIALOG:
				
				err = PopDialog(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:

				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_GLOBAL_SETDOWN:
				
				err = GlobalSetdown(in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_PARAMS_SETUP:

				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;
			
			case PF_Cmd_SEQUENCE_SETUP:
				
				err = SequenceSetup(in_data,
									out_data,
									params,
									output);
				break;
			
			case PF_Cmd_SEQUENCE_SETDOWN:
				
				err = SequenceSetdown(in_data,
									  out_data,
									  params,
									  output);
				break;
				
			case PF_Cmd_SEQUENCE_FLATTEN:
				
				err = SequenceFlatten(in_data, out_data);
				break;
			
			case PF_Cmd_SEQUENCE_RESETUP:
				
				err = SequenceResetup(in_data,out_data);
				break;
				
			case PF_Cmd_RENDER:

				err = Render(	in_data,
								out_data,
								params,
								output);
				break;
			
			case PF_Cmd_USER_CHANGED_PARAM:
				
				err = UserChangedParam(in_data,
									   out_data,
									   params,
									   reinterpret_cast<const PF_UserChangedParamExtra *>(extra));
			
			// Handling this selector will ensure that the UI will be properly initialized,
			// even before the user starts changing parameters to trigger PF_Cmd_USER_CHANGED_PARAM
			case PF_Cmd_UPDATE_PARAMS_UI:
				
				err = UpdateParameterUI(in_data,
										out_data,
										params,
										output);
				break;
				
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}

