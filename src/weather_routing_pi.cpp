/******************************************************************************
 *
 * Project:  OpenCPN Weather Routing plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2013 by Sean D'Epagnier                                 *
 *   sean@depagnier.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 */


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers


#include <wx/treectrl.h>
#include <wx/fileconf.h>

#include "BoatSpeed.h"
#include "RouteMap.h"
#include "weather_routing_pi.h"


GRIBUIDialog *g_pGribDialog = NULL;

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new weather_routing_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}


//---------------------------------------------------------------------------------------------------------
//
//    Weather_Routing PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "icons.h"



//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

weather_routing_pi::weather_routing_pi(void *ppimgr)
      :opencpn_plugin_18(ppimgr)
{
      // Create the PlugIn icons
      initialize_images();

      m_pWeather_RoutingDialog = NULL;
}

weather_routing_pi::~weather_routing_pi(void)
{
      delete _img_WeatherRouting;
}

int weather_routing_pi::Init(void)
{
      AddLocaleCatalog( _T("opencpn-weather_routing_pi") );

      //    Get a pointer to the opencpn configuration object
      m_pconfig = GetOCPNConfigObject();

      // Get a pointer to the opencpn display canvas, to use as a parent for the WEATHER_ROUTING dialog
      m_parent_window = GetOCPNCanvasWindow();

      m_leftclick_tool_id  = InsertPlugInTool(_T(""), _img_WeatherRouting, _img_WeatherRouting, wxITEM_CHECK,
                                              _("Weather_Routing"), _T(""), NULL,
                                              WEATHER_ROUTING_TOOL_POSITION, 0, this);

      wxMenu dummy_menu;
      m_startroute_menu_id = AddCanvasContextMenuItem
          (new wxMenuItem(&dummy_menu, -1, _("Start Weather Route")), this );
      SetCanvasContextMenuItemViz(m_startroute_menu_id, false);
      m_endroute_menu_id = AddCanvasContextMenuItem
          (new wxMenuItem(&dummy_menu, -1, _("End Weather Route")), this );
      SetCanvasContextMenuItemViz(m_endroute_menu_id, false);
#if 0
      OnToolbarToolCallback(m_leftclick_tool_id);
#endif

      //    And load the configuration items
      LoadConfig();

      return (WANTS_OVERLAY_CALLBACK |
              WANTS_OPENGL_OVERLAY_CALLBACK |
              WANTS_TOOLBAR_CALLBACK    |
              INSTALLS_TOOLBAR_TOOL     |
              INSTALLS_CONTEXTMENU_ITEMS |
              WANTS_CONFIG              |
              WANTS_CURSOR_LATLON       |
              WANTS_NMEA_EVENTS         |
              WANTS_PLUGIN_MESSAGING
            );
}

bool weather_routing_pi::DeInit(void)
{
      if(m_pWeather_RoutingDialog)
            m_pWeather_RoutingDialog->Close();

      return true;
}

int weather_routing_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int weather_routing_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int weather_routing_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int weather_routing_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxBitmap *weather_routing_pi::GetPlugInBitmap()
{
      return _img_WeatherRouting;
}

wxString weather_routing_pi::GetCommonName()
{
      return _("WeatherRouting");
}


wxString weather_routing_pi::GetShortDescription()
{
      return _("Weather Routing PlugIn for OpenCPN");
}


wxString weather_routing_pi::GetLongDescription()
{
      return _("Weather Routing PlugIn for OpenCPN\n\n\
Provides Weather routing features include:\n\
  automatic routing subject to various constraints:\n\
          islands or inverted regions (places we can't go in a boat)\n\
          optimal speed based on wind and currents\n\
          boat speed calculation\n\
          sail/power \n\
          \n\
          , great circle route, constrained routes, optimal routing. \n\
p");

}


void weather_routing_pi::SetDefaults(void)
{
}


int weather_routing_pi::GetToolbarToolCount(void)
{
      return 1;
}

void weather_routing_pi::SetCursorLatLon(double lat, double lon)
{
    if(m_pWeather_RoutingDialog)
        if(m_pWeather_RoutingDialog->m_routemap.SetCursorLatLon(lat, lon))
            RequestRefresh(m_parent_window);

    m_cursor_lat = lat;
    m_cursor_lon = lon;
}

void weather_routing_pi::SetPluginMessage(wxString &message_id, wxString &message_body)
{
    if(message_id == _T("GRIB_DIALOG"))
    {
        wxJSONReader r;
        wxJSONValue v;
        r.Parse(message_body, &v);

        wxString sptr = v[_T("GribDialogPtr")].AsString();
        wxCharBuffer bptr = sptr.To8BitData();
        const char* ptr = bptr.data();
        sscanf(ptr, "%p", &g_pGribDialog);
    }
}

void weather_routing_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex &pfix)
{
    m_boat_lat = pfix.Lat;
    m_boat_lon = pfix.Lon;
}

void weather_routing_pi::ShowPreferencesDialog( wxWindow* parent )
{
}

void weather_routing_pi::OnToolbarToolCallback(int id)
{
    if(!m_pWeather_RoutingDialog) {
        m_pWeather_RoutingDialog = new WeatherRoutingDialog(m_parent_window,
                                                            m_boat_lat, m_boat_lon);
        wxPoint p = m_pWeather_RoutingDialog->GetPosition();
        m_pWeather_RoutingDialog->Move(0,0);        // workaround for gtk autocentre dialog behavior
        m_pWeather_RoutingDialog->Move(p);

        SetCanvasContextMenuItemViz(m_startroute_menu_id, true);
        SetCanvasContextMenuItemViz(m_endroute_menu_id, true);
    }

    m_pWeather_RoutingDialog->Show(!m_pWeather_RoutingDialog->IsShown());
}

void weather_routing_pi::OnContextMenuItemCallback(int id)
{
    if(!m_pWeather_RoutingDialog)
        return;

    if(id == m_startroute_menu_id) {
        m_pWeather_RoutingDialog->m_tStartLat->SetValue(wxString::Format(_T("%f"), m_cursor_lat));
        m_pWeather_RoutingDialog->m_tStartLon->SetValue(wxString::Format(_T("%f"), m_cursor_lon));
    }

    if(id == m_endroute_menu_id) {
        m_pWeather_RoutingDialog->m_tEndLat->SetValue(wxString::Format(_T("%f"), m_cursor_lat));
        m_pWeather_RoutingDialog->m_tEndLon->SetValue(wxString::Format(_T("%f"), m_cursor_lon));
    }
}

bool weather_routing_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    return false;
}

bool weather_routing_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
    if(m_pWeather_RoutingDialog && m_pWeather_RoutingDialog->IsShown()) {
        m_pWeather_RoutingDialog->RenderRouteMap(vp);
        return true;
    } else
        return false;
}

bool weather_routing_pi::LoadConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      if(!pConf)
          return false;

      pConf->SetPath ( _T( "/PlugIns/WeatherRouting" ) );
      return true;
}

bool weather_routing_pi::SaveConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      if(!pConf)
          return false;

      pConf->SetPath ( _T ( "/PlugIns/WeatherRouting" ) );
      return true;
}

void weather_routing_pi::SetColorScheme(PI_ColorScheme cs)
{
      DimeWindow(m_pWeather_RoutingDialog);
}
