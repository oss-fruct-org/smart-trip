#include <unistd.h>
#include <signal.h>
#include <smartslog.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "ontology.h"
#include "common.h"
#include "st_point.h"

#include "loader.h"
#include "geo_common.h"
#include "wm-loader.h"


static bool create_loader(struct LoaderInterface* loader) {
    char* wmloader_key = get_config_value("config.ini", "WMLoader", "Key");
    bool created = true;

    if (wmloader_key != NULL) {
        *loader = create_wm_loader(wmloader_key);
    } else {
        created = false;
    }

    free(wmloader_key);

    return created;
}

int main(void) {
    struct LoaderInterface loader;

    init_rand();

    if (!create_loader(&loader)) {
        fprintf(stderr, "No point loader specified\n");
        return 1;
    }

	sslog_init();
    register_ontology();

    static char kp_name[1000];
    snprintf(kp_name, sizeof(kp_name), "geo_kp_%s", loader.get_name());

    sslog_node_t* node = create_node(kp_name, "config.ini");
	if (sslog_node_join(node) != SSLOG_ERROR_NO) {
		fprintf(stderr, "Can't join node\n");
		return 1;
	}

    geo_common_serve_kp(node, loader);

	sslog_node_leave(node);
	sslog_shutdown();
}