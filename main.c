#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <ctype.h>
#include <unistd.h>
#include <json.h>
#include "include/stringop.h"
#include "include/ipc-client.h"

#ifndef SWAYLAY_VERSION
	#define SWAYLAY_VERSION "undefined"
#endif

#include <signal.h>


char *socket_path = NULL;
char *identifier = NULL;
static volatile sig_atomic_t socketfd = 0;

void sig_handler(int _) {
	(void)_;
	printf("\n");
	close(socketfd);
	free(identifier);
	free(socket_path);
	socketfd = 0;
}

static bool success_object(json_object *result) {
	json_object *success;

	if (!json_object_object_get_ex(result, "success", &success)) {
		return true;
	}

	return json_object_get_boolean(success);
}

// Iterate results array and return false if any of them failed
static bool success(json_object *r, bool fallback) {
	if (!json_object_is_type(r, json_type_array)) {
		if (json_object_is_type(r, json_type_object)) {
			return success_object(r);
		}
		return fallback;
	}

	size_t results_len = json_object_array_length(r);
	if (!results_len) {
		return fallback;
	}

	for (size_t i = 0; i < results_len; ++i) {
		json_object *result = json_object_array_get_idx(r, i);

		if (!success_object(result)) {
			return false;
		}
	}

	return true;
}

const char* layout_from_response(struct json_object* obj) {
	struct json_object* o[3];
	json_bool exists = json_object_object_get_ex(obj, "input", &o[0]);
	if (!exists) {
		return NULL;
	}
	exists = json_object_object_get_ex(o[0], "identifier", &o[1]);
	if (!exists) {
		return NULL;
	}
	// If strings are different
	if (strcmp(json_object_get_string(o[1]), identifier)) {
		return NULL;
	}
	exists = json_object_object_get_ex(o[0],
			"xkb_active_layout_name", &o[2]);
	if (!exists) {
		return NULL;
	}
	const char* obj3 = json_object_get_string(o[2]);
	return obj3;
}

int main(int argc, char** argv) {
	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"socket", required_argument, NULL, 's'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0}
	};

	const char* const usage =
		"Usage: swaylay [options] [identifier]\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -s, --socket <socket>  Use the specified socket.\n"
		"  -v, --version          Show the version number and quit.\n"
		"\n"
		"Default identifier is 1:1:AT_Translated_Set_2_keyboard.\n";


	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hmpqrs:v", long_options,
				&option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 'v':
			fprintf(stdout, "swaylay version %s\n", SWAYLAY_VERSION);
			fprintf(stdout, "https://github.com/Hexawolf/swaylay\n");
			exit(EXIT_SUCCESS);
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			printf("Unable to retrieve socket path\n");
			free(socket_path);
			exit(EXIT_FAILURE);
		}
	}

	const uint32_t type = IPC_SUBSCRIBE;
	const char* const command = "[\"input\"]";
	socketfd = ipc_open_socket(socket_path);
	struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
	ipc_set_recv_timeout(socketfd, timeout);
	uint32_t len = strlen(command);
	char *resp = ipc_single_command(socketfd, type, command, &len);
	json_object *obj = json_tokener_parse(resp);
	if (obj == NULL) {
		fprintf(stderr, "ERROR: Could not parse json response from ipc."
			" This is a bug in sway.");
		printf("%s\n", resp);
		return 1;
	} else {
		if (!success(obj, true)) {
			printf("%s\n", json_object_to_json_string_ext(obj,
				JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
			return 1;
		}
		json_object_put(obj);
	}
	free(resp); resp = 0;

	if (optind < argc) {
		free(identifier);
		identifier = join_args(argv + optind, argc - optind);
	} else {
		// Default for many systems
		identifier = strdup("1:1:AT_Translated_Set_2_keyboard");
	}

	// Remove the timeout for subscribed events
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	ipc_set_recv_timeout(socketfd, timeout);
	signal(SIGINT, sig_handler);

	do {
		struct ipc_response* response = ipc_recv_response(socketfd);
		if (!response) {
			break;
		}
		const char* lay = NULL;
		json_object* obj = json_tokener_parse(response->payload);
		if (obj != NULL) lay = layout_from_response(obj);
		if (lay != NULL) {
			printf("%c%c\n", lay[0], toupper(lay[1]));
			fflush(stdout);
		}
		if (obj != NULL) json_object_put(obj);
		free_ipc_response(response);
	} while (socketfd);
	
	return 0;
}
