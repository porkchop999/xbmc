/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "settings/lib/ISettingCallback.h"
#include "settings/lib/ISettingsHandler.h"

#include <map>
#include <string>
#include <vector>

class CSetting;
struct StringSettingOption;

class CPosixTimezone : public ISettingCallback, public ISettingsHandler
{
public:
   CPosixTimezone() = default;

   void Init();

   void OnSettingChanged(std::shared_ptr<const CSetting> setting) override;

   void OnSettingsLoaded() override;

   std::string GetOSConfiguredTimezone();

   std::vector<std::string> GetCountries();
   std::vector<std::string> GetTimezonesByCountry(const std::string& country);
   std::string GetCountryByTimezone(const std::string& timezone);

   void SetTimezone(std::string timezone);
   int m_IsDST = 0;

   static void SettingOptionsTimezoneCountriesFiller(std::shared_ptr<const CSetting> setting, std::vector<StringSettingOption> &list, std::string &current, void *data);
   static void SettingOptionsTimezonesFiller(std::shared_ptr<const CSetting> setting, std::vector<StringSettingOption> &list, std::string &current, void *data);

private:
   std::vector<std::string> m_countries;
   std::map<std::string, std::string> m_countryByCode;
   std::map<std::string, std::string> m_countryByName;

   std::map<std::string, std::vector<std::string> > m_timezonesByCountryCode;
   std::map<std::string, std::string> m_countriesByTimezoneName;
};

extern CPosixTimezone g_timezone;

