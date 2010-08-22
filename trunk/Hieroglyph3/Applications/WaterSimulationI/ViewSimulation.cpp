//--------------------------------------------------------------------------------
// This file is a portion of the Hieroglyph 3 Rendering Engine.  It is distributed
// under the MIT License, available in the root of this distribution and 
// at the following URL:
//
// http://www.opensource.org/licenses/mit-license.php
//
// Copyright (c) 2003-2010 Jason Zink 
//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
#include "ViewSimulation.h"
#include "Entity3D.h"
#include "Node3D.h"
#include "Texture2dConfigDX11.h"
#include "Log.h"
#include <sstream>
#include "ParameterManagerDX11.h"
//--------------------------------------------------------------------------------
using namespace Glyph3;
//--------------------------------------------------------------------------------
ViewSimulation::ViewSimulation( RendererDX11& Renderer, int SizeX, int SizeY )
{
	m_sParams.iViewType = VT_SIMULATION;

	// Remember what number of thread groups to run in the dispatch call.

	ThreadGroupsX = SizeX;
	ThreadGroupsY = SizeY;

	int PointCountX = SizeX * 16;
	int PointCountY = SizeY * 16;

	// Prepare some intial state data for the simulation.

	GridPoint* pData = new GridPoint[PointCountX*PointCountY];
	for ( int j = 0; j < PointCountY; j++ )
	{
		for ( int i = 0; i < PointCountX; i++ )
		{
			int x = i - 32;
			int y = j - 96;

			const float fFrequency = 0.1f;

			if ( x*x+y*y != 0.0f )
			{
				pData[PointCountX*j+i].height = 40.0f * sinf( sqrt( (float)(x*x+y*y) ) * fFrequency ) / ( sqrt( (float)(x*x+y*y) ) * fFrequency );
				pData[PointCountX*j+i].flow.MakeZero();
			}
			else
			{
				pData[PointCountX*j+i].height = 40.0f;
				pData[PointCountX*j+i].flow.MakeZero();
			}
		}
	}

	D3D11_SUBRESOURCE_DATA InitialData;
	InitialData.pSysMem				= pData;
	InitialData.SysMemPitch			= 0;
	InitialData.SysMemSlicePitch	= 0;

	// Create the resources that will hold the state of the water heights 
	// and flows, using the initial data created above.

	BufferConfigDX11 config;
	config.SetDefaultStructuredBuffer( PointCountX * PointCountY, sizeof( GridPoint ) );

	WaterState[0] = Renderer.CreateStructuredBuffer( &config, &InitialData );
	WaterState[1] = Renderer.CreateStructuredBuffer( &config, &InitialData );

	// Release the system memory since the simulation is now going to be run on
	// the GPU :)

	delete[] pData;

	// Set up the render effect for actually updating the simulation.

	pWaterEffect = new RenderEffectDX11();
	pWaterEffect->m_iComputeShader = 
		Renderer.LoadShader( COMPUTE_SHADER,
		std::wstring( L"../Data/Shaders/WaterSimulation.hlsl" ),
		std::wstring( L"CSMAIN" ),
		std::wstring( L"cs_4_0" ) );
}
//--------------------------------------------------------------------------------
ViewSimulation::~ViewSimulation()
{
	SAFE_DELETE( pWaterEffect );
}
//--------------------------------------------------------------------------------
void ViewSimulation::Update( float fTime )
{
}
//--------------------------------------------------------------------------------
void ViewSimulation::PreDraw( RendererDX11* pRenderer )
{
	// Queue this view into the renderer for processing.  Since this is a 
	// simulation style view, there is no root and hence no additional recursive
	// 'PreDraw'ing required.
	pRenderer->QueueRenderView( this );
}
//--------------------------------------------------------------------------------
void ViewSimulation::Draw( PipelineManagerDX11* pPipelineManager, ParameterManagerDX11* pParamManager )
{
	// Set this view's render parameters.
	SetRenderParams( pParamManager );

	// Perform the dispatch call to update the simulation state.
	pPipelineManager->Dispatch( *pWaterEffect, ThreadGroupsX, ThreadGroupsY, 1, pParamManager );

	// Switch the two resources so that the current state is maintained in slot 0.
	ResourcePtr TempState = WaterState[0];
	WaterState[0] = WaterState[1];
	WaterState[1] = TempState;
}
//--------------------------------------------------------------------------------
void ViewSimulation::SetRenderParams( ParameterManagerDX11* pParamManager )
{
	// Set the parameters for this view to be able to perform its processing
	// sequence.  In this case, water state '0' is always considered the current
	// state.

	pParamManager->SetShaderResourceParameter( L"CurrentWaterState", WaterState[0] );
	pParamManager->SetUnorderedAccessParameter( L"NewWaterState", WaterState[1] );
}
//--------------------------------------------------------------------------------
void ViewSimulation::SetUsageParams( ParameterManagerDX11* pParamManager )
{
	// Set the parameters for allowing an application to use the current state
	// as a height map via a shader resource view.

	Vector4f DispatchSize = Vector4f( 16.0f, 16.0f, 16.0f * 16.0f, 16.0f * 16.0f );
	pParamManager->SetVectorParameter( std::wstring( L"DispatchSize" ), &DispatchSize );

	pParamManager->SetShaderResourceParameter( L"CurrentWaterState", WaterState[0] );
}
//--------------------------------------------------------------------------------