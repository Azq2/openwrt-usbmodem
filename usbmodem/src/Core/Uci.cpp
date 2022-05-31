#include "Uci.h"
#include "Log.h"

#include <uci.h>
#include <cstring>

bool Uci::loadIfaceConfig(const std::string &iface, std::map<std::string, std::string> *options) {
	uci_context *context = uci_alloc_context();
	uci_package *package = nullptr;
	
	if (!context)
		return false;
	
	if (uci_load(context, "network", &package) != UCI_OK) {
		uci_perror(context, "uci_load()");
		uci_free_context(context);
		return false;
	}
	
	auto section = uci_lookup_section(context, package, iface.c_str());
	if (!section || strcmp(section->type, "interface") != 0) {
		LOGE("Can't find config network.%s\n", iface.c_str());
		uci_free_context(context);
		return false;
	}
	
	uci_element *option_el;
	uci_foreach_element(&section->options, option_el) {
		uci_option *option = uci_to_option(option_el);
		if (option->type == UCI_TYPE_STRING)
			options->insert_or_assign(option_el->name, option->v.string);
	};
	
	uci_free_context(context);
	
	return true;
}

bool Uci::loadIfaceFwZone(const std::string &iface, std::string *zone) {
	uci_context *context = uci_alloc_context();
	uci_package *package = nullptr;
	
	// Default fw3 zone
	zone->assign("");
	
	if (!context)
		return false;
	
	if (uci_load(context, "firewall", &package) != UCI_OK) {
		uci_perror(context, "uci_load()");
		uci_free_context(context);
		return false;
	}
	
	uci_element *section_el, *option_el, *list_el;
	uci_foreach_element(&package->sections, section_el) {
		uci_section *section = uci_to_section(section_el);
		
		if (strcmp(section->type, "zone") == 0) {
			std::string zone_name;
			bool found = false;
			
			uci_foreach_element(&section->options, option_el) {
				uci_option *option = uci_to_option(option_el);
				if (option->type == UCI_TYPE_STRING) {
					if (strcmp(option_el->name, "name") == 0)
						zone_name = option->v.string;
				} else if (option->type == UCI_TYPE_LIST) {
					if (strcmp(option_el->name, "network") == 0) {
						uci_foreach_element(&option->v.list, list_el) {
							if (strcmp(list_el->name, iface.c_str()) == 0) {
								found = true;
								break;
							}
						}
					}
				}
			}
			
			if (found) {
				zone->assign(zone_name);
				break;
			}
		}
	}
	
	uci_free_context(context);
	
	return true;
}
