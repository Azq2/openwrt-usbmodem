#include "Uci.h"
#include "Utils.h"
#include "Log.h"

#include <uci.h>
#include <cstring>

std::vector<Uci::Section> Uci::loadSections(const std::string &pkg_name, const std::string &type) {
	uci_context *context = uci_alloc_context();
	if (!context)
		throw new std::runtime_error("uci_alloc_context()");
	
	uci_package *pkg = nullptr;
	if (uci_load(context, pkg_name.c_str(), &pkg) != UCI_OK) {
		uci_perror(context, "uci_load()");
		throw new std::runtime_error("uci_load()");
	}
	
	std::vector<Section> result;
	
	uci_element *section_el, *option_el, *list_el;
	uci_foreach_element(&pkg->sections, section_el) {
		uci_section *section_ref = uci_to_section(section_el);
		
		if (type.size() && strcmp(section_ref->type, type.c_str()) != 0)
			continue;
		
		result.resize(result.size() + 1);
		Section &section_info = result.back();
		
		section_info.name = section_el->name;
		section_info.type = section_ref->type;
		
		uci_foreach_element(&section_ref->options, option_el) {
			uci_option *option_ref = uci_to_option(option_el);
			if (option_ref->type == UCI_TYPE_STRING) {
				section_info.options[option_el->name] = option_ref->v.string;
			} else if (option_ref->type == UCI_TYPE_LIST) {
				uci_foreach_element(&option_ref->v.list, list_el) {
					section_info.lists[option_el->name].push_back(list_el->name);
				}
			}
		}
	}
	
	uci_free_context(context);
	
	return result;
}

std::string Uci::getFirewallZone(const std::string &iface) {
	for (auto &section: loadSections("firewall", "zone")) {
		if (hasMapKey(section.lists, "network")) {
			for (auto &net: section.lists["network"]) {
				if (net == iface)
					return getMapValue(section.options, "name", "");
			}
		}
	}
	return "";
}

std::tuple<bool, Uci::Section> Uci::loadSectionByName(const std::string &pkg_name, const std::string &type, const std::string &name) {
	for (auto &section: loadSections(pkg_name, type)) {
		if (section.name == name)
			return {true, section};
	}
	return {false, {}};
}
