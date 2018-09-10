/*
 * what3words grabber
 * Turn an area bounded by a two coordinates (NE and SW corners) into
 * a list of coordinates of grid squares as defined by the w3w API
 * GPLv3. See LICENSE.txt
 *
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <json-c/json.h>

/**
 * Execute a GET request and put the result into a write_buf struct
 *
 * @param request
 * @param write_buf a void* ptr to a write_buf struct
 * @returns status (0=OK)
 */
CURLcode query(char *request, void *write_buf);

/**
 * Struct to hold the results of a CURL query
 */
struct write_buf {
	size_t size;
	char* buf;
};

void write_buf_init(struct write_buf *);
void write_buf_reset(struct write_buf *);
void write_buf_cleanup(struct write_buf *);

/**
 * Callback function passed to CURL to fill a write_buf struct.
 */
static size_t write_callback(void* buf, size_t sz, size_t nmemb, void *userp);

/**
 * Struct for a linked list of coordinates (lat/long) returned by
 * the what3words 'grid' request
 */
struct ll_coord {
	double lat;
	double lng;
	struct ll_coord* next;
};

struct ll_coord* ll_coord_create(struct ll_coord *);
void ll_coord_cleanup(struct ll_coord *);

/**
 * Turn a response string from the 'grid' API request into a linked list
 * by parsing the JSON response
 *
 * @param str the HTTP response
 * @returns ptr to head of the linked list.
 */
struct ll_coord* grid_resp_str_to_coords(char* str)
{
	json_object *resp;
	resp = json_tokener_parse(str);
	json_object *lines;
	int rc = json_object_object_get_ex(resp, "lines", &lines);
	if(rc == 0){
		printf("Error: invalid grid response received");
		json_object_put(resp);
		return NULL;
	}
	size_t ngrids = json_object_array_length(lines);
	printf("%zu results returned\n", ngrids);
	struct ll_coord *head = NULL;
	for(int i=0; i<ngrids; ++i){
		head = ll_coord_create(head);
		json_object *it = json_object_array_get_idx(lines, i);
		enum json_type type;
		type = json_object_get_type(it);
		if(type != json_type_object){
			printf("Error: unexpected non-object in list");
			json_object_put(resp);
			return NULL;
		}
		json_object *p, *q;
		rc = json_object_object_get_ex(it, "start", &p);
		if(rc){
			rc = json_object_object_get_ex(p, "lat", &q);
			if(rc){
				head->lat = json_object_get_double(q);
			}
			rc = json_object_object_get_ex(p, "lng", &q);
			if(rc){
				head->lng = json_object_get_double(q);
			}
		}
	}
	json_object_put(resp);
	return head;
}

int main(int argc, char** argv)
{
	const size_t REQ_STR_SZ = 120;
	char* req_fmt = "https://api.what3words.com/v2/"
			"grid?bbox=%s&format=json&key=%s";
	if(argc != 3){
		printf("Usage: %s <bbox> <api key>\n", argv[0]);
		return -1;
	}
	char req[REQ_STR_SZ];
	snprintf(req, REQ_STR_SZ - 1, req_fmt, argv[1], argv[2]);
	struct write_buf wb;
	write_buf_init(&wb);
	CURLcode success = query(req, (void*) &wb);
	if(success != 0){
		printf("Error: CURL query failed\n");
		return -1;
	}
	struct ll_coord* head = grid_resp_str_to_coords(wb.buf);
	write_buf_reset(&wb);
	struct ll_coord* p = head;
	while(p != NULL){
		// do something with the coordinates - in this case, print
		printf("Square: lat=%f, long=%f\n", p->lat, p->lng);
		p = p->next;
	}
	ll_coord_cleanup(head);
	write_buf_cleanup(&wb);
	return 0;
}

void write_buf_init(struct write_buf* wb)
{
	wb->size = 0;
	wb->buf = NULL;
}

void write_buf_cleanup(struct write_buf* wb)
{
	free(wb->buf);
	wb->buf = NULL;
}

void write_buf_reset(struct write_buf* wb)
{
	write_buf_cleanup(wb);
	write_buf_init(wb);
}
static size_t write_callback(void* buf,  size_t sz, size_t n, void* p)
{
	struct write_buf* wb = (struct write_buf*) p;
	size_t len = sz * n;
	if(wb->buf == NULL){
		assert(wb->size == 0);
		wb->buf = malloc(len+1);
	} else {
		wb->buf = realloc(wb->buf, wb->size + len + 1);
	}
	if(wb->buf){
		memcpy(wb->buf + wb->size, buf, len);
		wb->size += len;
		wb->buf[wb->size] = 0;
	}
	return len;
}

struct ll_coord* ll_coord_create(struct ll_coord *tail)
{
	struct ll_coord* p = malloc(sizeof(struct ll_coord));
	p->next = tail;
	return p;
}

void ll_coord_cleanup(struct ll_coord* first)
{
	struct ll_coord* p = first;
	while(p != NULL){
		struct ll_coord* q = p->next;
		free(p);
		p = q;
	}
}

CURLcode query(char* request, void* write_buf)
{
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, request);
	curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, write_buf);
	CURLcode success = curl_easy_perform(handle);
	curl_easy_cleanup(handle);
	return success;
}
