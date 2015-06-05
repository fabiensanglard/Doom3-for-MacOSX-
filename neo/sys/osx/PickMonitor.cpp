/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "../../idlib/precompiled.h"
#include <Carbon/Carbon.h>
#include "PickMonitor.h"

//====================================================================================
//	CONSTANTS
//====================================================================================

#define kMaxMonitors		16

//====================================================================================
//	TYPES
//====================================================================================

typedef struct
{
	CGDirectDisplayID	device;
	CGRect				origRect;
	CGRect				scaledRect;
	int					isMain;
}
Monitor;


//====================================================================================
//	GLOBALS
//====================================================================================
static CGDirectDisplayID sSelectedDevice;
static int sNumMonitors;
static Monitor sMonitors[kMaxMonitors];

//====================================================================================
//	IMPLEMENTATION
//====================================================================================

//-----------------------------------------------------------------------------
//	SetupUserPaneProcs
//-----------------------------------------------------------------------------
// 	Call this to initialize the specified user pane control before displaying 
//	the dialog window. Pass NULL for any user pane procs you don't need to install.

OSErr SetupUserPaneProcs(	ControlRef inUserPane,
							ControlUserPaneHitTestProcPtr inHitTestProc,
							ControlUserPaneTrackingProcPtr inTrackingProc)
{
	OSErr	err = noErr;
	ControlUserPaneHitTestUPP hitTestUPP;
	ControlUserPaneTrackingUPP trackingUPP;
	
	if (0 == inUserPane) return paramErr;
	
	if (inHitTestProc && noErr == err)
	{
		hitTestUPP = NewControlUserPaneHitTestUPP(inHitTestProc);

		if (0 == hitTestUPP)
			err = memFullErr;
		else
			err = SetControlData(	inUserPane,
									kControlEntireControl, 
									kControlUserPaneHitTestProcTag,
									sizeof(ControlUserPaneHitTestUPP),
									(Ptr)&hitTestUPP);
	}
	if (inTrackingProc && noErr == err)
	{
		trackingUPP = NewControlUserPaneTrackingUPP(inTrackingProc);
		
		if (0 == trackingUPP)
			err = memFullErr;
		else
			err = SetControlData(	inUserPane,
									kControlEntireControl, 
									kControlUserPaneTrackingProcTag,
									sizeof(ControlUserPaneTrackingUPP),
									(Ptr)&trackingUPP);
	}
	
	return err;
}


//-----------------------------------------------------------------------------
//	DisposeUserPaneProcs
//-----------------------------------------------------------------------------
// 	Call this to clean up when you're done with the specified user pane control.

OSErr DisposeUserPaneProcs(ControlRef inUserPane)
{	
	ControlUserPaneHitTestUPP hitTestUPP;
	ControlUserPaneTrackingUPP trackingUPP;
	Size actualSize;
	OSErr err;
	
	err = GetControlData(inUserPane, kControlEntireControl, kControlUserPaneHitTestProcTag, sizeof(ControlUserPaneHitTestUPP), (Ptr)&hitTestUPP, &actualSize);
	if (err == noErr) DisposeControlUserPaneHitTestUPP(hitTestUPP);

	err = GetControlData(inUserPane, kControlEntireControl, kControlUserPaneTrackingProcTag, sizeof(ControlUserPaneTrackingUPP), (Ptr)&trackingUPP, &actualSize);
	if (err == noErr) DisposeControlUserPaneTrackingUPP(trackingUPP);

	return noErr;
}

#pragma mark -

//-----------------------------------------------------------------------------
//	hitTestProc
//-----------------------------------------------------------------------------
//	Custom hitTestProc for our UserPane control.
//	This allows FindControlUnderMouse() to locate our control, which allows
//	ModalDialog() to call TrackControl() or HandleControlClick() for our control.

static pascal ControlPartCode hitTestProc(ControlRef inControl, Point inWhere)
{
	// return a valid part code so HandleControlClick() will be called
	return kControlButtonPart;
}


//-----------------------------------------------------------------------------
//	trackingProc
//-----------------------------------------------------------------------------
//	Custom trackingProc for our UserPane control.
//	This won't be called for our control unless the kControlHandlesTracking feature
//	bit is specified when the userPane is created.

static pascal ControlPartCode trackingProc (
					ControlRef inControl,
					Point inStartPt,
					ControlActionUPP inActionProc)
{
	#pragma unused (inControl, inStartPt, inActionProc)
	int i;
	CGPoint point;

	point = CGPointMake(inStartPt.h, inStartPt.v);

	for (i = 0; i < sNumMonitors; i++)
	{
		if (CGRectContainsPoint(sMonitors[i].scaledRect, point))
		{
			if (sMonitors[i].device != sSelectedDevice)
			{
				sSelectedDevice = sMonitors[i].device;
				DrawOneControl(inControl);
			}
			break;
		}
	}
	
	return kControlNoPart;
}


#pragma mark -


//-----------------------------------------------------------------------------
//	SetupPickMonitorPane
//-----------------------------------------------------------------------------
//	Call this to initialize the user pane control that is the Pick Monitor
//	control. Pass the ControlRef of the user pane control and a display ID
//	for the monitor you want selected by default (pass 0 for the main monitor).
//	Call this function before displaying the dialog window.

OSErr SetupPickMonitorPane(ControlRef inPane, CGDirectDisplayID inDefaultMonitor)
{
	CGDirectDisplayID displays[kMaxMonitors];
	CGDisplayCount displayCount;
	OSErr err = noErr;
	int i;
	
	// make the default monitor the selected device
	if (inDefaultMonitor)
		sSelectedDevice = inDefaultMonitor;
	else
		sSelectedDevice = CGMainDisplayID();
	
	// build the list of monitors
	sNumMonitors = 0;
	if (CGGetActiveDisplayList(kMaxMonitors, displays, &displayCount) == CGDisplayNoErr)
	{
		for (i = 0; i < displayCount; i++)
		{
			HIShapeRef shape;
			CGRect r;
			
			HIWindowCopyAvailablePositioningShape(displays[i], kHICoordSpaceScreenPixel, &shape);
			HIShapeGetBounds(shape, &r);
			
			sMonitors[i].device = displays[i];
			sMonitors[i].isMain = (displays[i] == CGMainDisplayID());
			sMonitors[i].origRect = r;
			
			sNumMonitors++;
		}
	}
	
	// calculate scaled rects
	if (sNumMonitors)
	{
		CGRect origPaneRect, paneRect;
		CGRect origGrayRect, grayRect, scaledGrayRect;
		CGFloat srcAspect, dstAspect, scale, dx, dy;
		
		HIViewGetBounds(inPane, &origPaneRect);
		
		paneRect = origPaneRect;
		paneRect = CGRectOffset(paneRect, -paneRect.origin.x, -paneRect.origin.y);
		
		origGrayRect = sMonitors[0].origRect;
		for (i = 1 ; i < sNumMonitors; i++)
		{
			origGrayRect = CGRectUnion(origGrayRect, sMonitors[i].origRect);
		}
		grayRect = origGrayRect;
		grayRect = CGRectOffset(grayRect, -grayRect.origin.x, -grayRect.origin.y);
		
		srcAspect = grayRect.size.width / grayRect.size.height;
		dstAspect = paneRect.size.width / paneRect.size.height;
		
		scaledGrayRect = paneRect;
		if (srcAspect < dstAspect)
		{
			scaledGrayRect.size.width = paneRect.size.height * srcAspect;
			scale = scaledGrayRect.size.width / grayRect.size.width;
		}
		else
		{
			scaledGrayRect.size.height = paneRect.size.width / srcAspect;
			scale = scaledGrayRect.size.height / grayRect.size.height;
		}
		
		for (i = 0; i < sNumMonitors; i++)
		{
			CGRect r = sMonitors[i].origRect;
			CGRect r2 = r;
			
			// normalize rect and scale
			r = CGRectOffset(r, -r.origin.x, -r.origin.y);
			r.size.height *= scale;
			r.size.width *= scale;
			
			// offset rect wrt gray region
			dx = (r2.origin.x - origGrayRect.origin.x) * scale;
			dy = (r2.origin.y - origGrayRect.origin.y) * scale;
			r = CGRectOffset(r, dx, dy);
			
			sMonitors[i].scaledRect = r;
		}
		
		// center scaledGrayRect in the pane
		dx = (paneRect.size.width - scaledGrayRect.size.width) / 2.0f;
		dy = (paneRect.size.height - scaledGrayRect.size.height) / 2.0f;
		scaledGrayRect = CGRectOffset(scaledGrayRect, dx, dy);
		
		// offset monitors to match
		for (i = 0; i < sNumMonitors; i++)
		{
			sMonitors[i].scaledRect = CGRectOffset(sMonitors[i].scaledRect,
												   scaledGrayRect.origin.x,
												   scaledGrayRect.origin.y);
		}
	}
	else
		return paramErr;
	
	// setup the procs for the pick monitor user pane
	err = SetupUserPaneProcs(inPane,  hitTestProc, trackingProc);
	return err;
}


//-----------------------------------------------------------------------------
//	TearDownPickMonitorPane
//-----------------------------------------------------------------------------
//	Disposes of everything associated with the Pick Monitor pane. You should
//	call this when disposing the dialog.

OSErr TearDownPickMonitorPane(ControlRef inPane)
{
	OSErr err;
	err = DisposeUserPaneProcs(inPane);
	sNumMonitors = 0;
	return err;
}

#pragma mark -

//------------------------------------------------------------------------------------
// ¥ DrawPaneHandler
//------------------------------------------------------------------------------------
// Our draw handler for the PickMonitor dialog.

static pascal OSStatus DrawPaneHandler( EventHandlerCallRef inHandler, EventRef inEvent, void* inUserData )
{
	OSStatus result = eventNotHandledErr;
	
	// draw system control
	result = CallNextEventHandler(inHandler, inEvent);
	
	if (result == noErr && GetEventKind( inEvent ) == kEventControlDraw)
	{
		CGContextRef context;
		ControlRef control;
		int i;
		
		// get control
		GetEventParameter(inEvent, kEventParamDirectObject, typeControlRef, NULL, sizeof(control), NULL, &control);
		
		// get context
		GetEventParameter(inEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof(context), NULL, &context);
		
		for (i = 0; i < sNumMonitors; i++)
		{
			CGContextSetRGBFillColor(context, 0.3215686275f, 0.5411764706f, 0.8f, 1.0f);
			CGContextFillRect(context, sMonitors[i].scaledRect);
			
			if (sMonitors[i].isMain)
			{
				CGRect r = sMonitors[i].scaledRect;
				r = CGRectInset(r, 1, 1);
				r.size.height = 6;
				CGContextSetRGBFillColor(context, 1.0f, 1.0f, 1.0f, 1.0f);
				CGContextFillRect(context, r);
				CGContextSetLineWidth(context, 1.0f);
				CGContextMoveToPoint(context, r.origin.x, r.origin.y + r.size.height);
				CGContextAddLineToPoint(context, r.origin.x + r.size.width, r.origin.y + r.size.height);
				CGContextStrokePath(context);
			}
			if (sMonitors[i].device == sSelectedDevice)
			{
				CGContextSetLineWidth(context, 3.0f);
				CGContextSetRGBStrokeColor(context, 0.0f, 0.0f, 0.0f, 1.0f);
				CGContextStrokeRect(context, CGRectInset(sMonitors[i].scaledRect, 1.5f, 1.5f));
			}
			else
			{
				CGContextSetLineWidth(context, 1.0f);
				CGContextSetRGBStrokeColor(context, 0.0f, 0.0f, 0.0f, 1.0f);
				CGContextStrokeRect(context, sMonitors[i].scaledRect);
			}
		}
	}
	
	
	return result;
}

//------------------------------------------------------------------------------------
// ´ PickMonitorHandler
//------------------------------------------------------------------------------------
// Our command handler for the PickMonitor dialog.

static pascal OSStatus PickMonitorHandler( EventHandlerCallRef inHandler, EventRef inEvent, void* inUserData )
{
	#pragma unused( inHandler )
	
	HICommand			cmd;
	OSStatus			result = eventNotHandledErr;
	WindowRef			theWindow = (WindowRef)inUserData;

	// The direct object for a 'process commmand' event is the HICommand.
	// Extract it here and switch off the command ID.

	GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof( cmd ), NULL, &cmd );

	switch ( cmd.commandID )
	{
		case kHICommandOK:			
			QuitAppModalLoopForWindow( theWindow );
			result = noErr;
			break;
		
		case kHICommandCancel:			
			// Setting sSelectedDevice to zero will signal that the user cancelled.
			sSelectedDevice = 0;
			QuitAppModalLoopForWindow( theWindow );
			result = noErr;
			break;

	}	
	return result;
}


#pragma mark -

//-----------------------------------------------------------------------------
// CanUserPickMonitor
//-----------------------------------------------------------------------------
// Returns true if more than one monitor is available to choose from.

Boolean CanUserPickMonitor (void)
{
	CGDirectDisplayID displays[kMaxMonitors];
	OSErr err = noErr;
	CGDisplayCount numMonitors;
	
	// build the list of monitors
	err = CGGetActiveDisplayList(kMaxMonitors, displays, &numMonitors);

	if (err == CGDisplayNoErr && numMonitors > 1) return true;
	else return false;
}

//-----------------------------------------------------------------------------
// PickMonitor
//-----------------------------------------------------------------------------
// Prompts for a monitor. Returns userCanceledErr if the user cancelled.

OSStatus PickMonitor (CGDirectDisplayID *inOutDisplayID, WindowRef parentWindow)
{
	WindowRef theWindow;
	OSStatus status = noErr;
	static const ControlID	kUserPane 		= { 'MONI', 1 };
	
	// Fetch the dialog

	IBNibRef aslNib;
	CFBundleRef theBundle = CFBundleGetMainBundle();
	status = CreateNibReferenceWithCFBundle(theBundle, CFSTR("ASLCore"), &aslNib);
	status = ::CreateWindowFromNib(aslNib, CFSTR( "Pick Monitor" ), &theWindow );
	if (status != noErr)
	{
		assert(false);
		return userCanceledErr;
	}

#if 0
	// Put game name in window title. By default the title includes the token <<<kGameName>>>.

	Str255 windowTitle;
	GetWTitle(theWindow, windowTitle);
	FormatPStringWithGameName(windowTitle);
	SetWTitle(theWindow, windowTitle);
#endif
		
	// Set up the controls

	ControlRef monitorPane;
	GetControlByID( theWindow, &kUserPane, &monitorPane );
	assert(monitorPane);

	SetupPickMonitorPane(monitorPane, *inOutDisplayID);

	// Create our UPPs and install the handlers.

	EventTypeSpec cmdEventPick = { kEventClassCommand, kEventCommandProcess };
	EventHandlerUPP handlerPick = NewEventHandlerUPP( PickMonitorHandler );
	InstallWindowEventHandler( theWindow, handlerPick, 1, &cmdEventPick, theWindow, NULL );
	
	EventTypeSpec cmdEventDraw = { kEventClassControl, kEventControlDraw };
	EventHandlerUPP handlerDraw = NewEventHandlerUPP( DrawPaneHandler );
	InstallEventHandler( GetControlEventTarget( monitorPane ), handlerDraw, 1, &cmdEventDraw, NULL, NULL );
	
	// Show the window

	if (parentWindow)
		ShowSheetWindow( theWindow, parentWindow );
	else
		ShowWindow( theWindow );

	// Now we run modally. We will remain here until the PrefHandler
	// calls QuitAppModalLoopForWindow if the user clicks OK or
	// Cancel.

	RunAppModalLoopForWindow( theWindow );

	// OK, we're done. Dispose of our window and our UPP.
	// We do the UPP last because DisposeWindow can send out
	// CarbonEvents, and we haven't explicitly removed our
	// handler. If we disposed the UPP, the Toolbox might try
	// to call it. That would be bad.

	TearDownPickMonitorPane(monitorPane);
	if (parentWindow)
		HideSheetWindow( theWindow );
	DisposeWindow( theWindow );
	DisposeEventHandlerUPP( handlerPick );
	DisposeEventHandlerUPP( handlerDraw );

	// Return settings to caller

	if (sSelectedDevice != 0)
	{
		// Read back the controls
		*inOutDisplayID = sSelectedDevice;
		return noErr;
	}
	else
		return userCanceledErr;

}

