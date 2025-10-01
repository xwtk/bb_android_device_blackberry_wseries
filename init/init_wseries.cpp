/*
   Copyright (c) 2024, The LineageOS Project

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <string>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include "vendor_init.h"

void property_override(char const prop[], char const value[]) {
    prop_info* pi = (prop_info*)__system_property_find(prop);
    if (pi) {
        __system_property_update(pi, value, strlen(value));
    }
}

void vendor_load_properties() {
    std::string binfo_product = android::base::GetProperty("ro.boot.binfo.product", "");
    if (binfo_product.empty())
    	binfo_product = android::base::GetProperty("ro.boot.binfo.family", "");
    std::string binfo_name = android::base::GetProperty("ro.boot.binfo.name", "");
    if (binfo_name.empty())
    	binfo_name = android::base::GetProperty("ro.boot.binfo.device", "");
    
    std::string model;
    std::string codename;

    if (binfo_product == "windermere" || binfo_product == "wolverine") {
        if (binfo_name == (binfo_product + "wichita")) {
            model = "Passport (Wichita)";
            codename = "wichita";
        } else {
            model = "Passport";
            codename = "wolverine";
        }
    } else if (binfo_product == "oslo") {
        model = "Passport Silver Edition";
        codename = "oslo";
    } else if (binfo_product == "keian") {
        model = "Passport Porsche Edition";
        codename = "keian";
    }

    if (!model.empty()) {
	    property_override("ro.product.model", model.c_str());
	    property_override("ro.product.odm.model", model.c_str());
	    property_override("ro.product.product.model", model.c_str());
	    property_override("ro.product.system.model", model.c_str());
	    property_override("ro.product.system_ext.model", model.c_str());
	    property_override("ro.product.vendor.model", model.c_str());

	    property_override("ro.build.product", codename.c_str());
	    property_override("ro.product.board", codename.c_str());
	    property_override("ro.product.device", codename.c_str());
	    property_override("ro.product.name", codename.c_str());
	    property_override("ro.product.odm.device", codename.c_str());
	    property_override("ro.product.odm.name", codename.c_str());
	    property_override("ro.product.product.device", codename.c_str());
	    property_override("ro.product.product.name", codename.c_str());
	    property_override("ro.product.system.device", codename.c_str());
	    property_override("ro.product.system.name", codename.c_str());
	    property_override("ro.product.system_ext.device", codename.c_str());
	    property_override("ro.product.system_ext.name", codename.c_str());
	    property_override("ro.product.vendor.device", codename.c_str());
	    property_override("ro.product.vendor.name", codename.c_str());
    }
}
