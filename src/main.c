/*
 * main.c
 *
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <json-c/json.h>

struct write_buf {
	size_t size;
	char* buf;
};

void write_buf_init(struct write_buf*);
void write_buf_cleanup(struct write_buf*);
static size_t write_callback(void* buf, size_t sz, size_t nmemb, void* userp);

struct ll_coord {
	double lat;
	double lng;
	struct ll_coord* next;
};

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

CURLcode query(char* request, void* curl_writedata)
{
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, request);
	curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, curl_writedata);
	CURLcode success = curl_easy_perform(handle);
	curl_easy_cleanup(handle);
	return success;
}

struct ll_coord* get_coords_from_grid_resp_str(char* str)
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
		printf("%f\n", head->lat);
	}
	json_object_put(resp);
	return head;
}

int main(int argc, char** argv)
{
	char* req = "https://api.what3words.com/v2/grid?"
		"bbox=52.208867,0.117540,52.207988,0.116126"
		"&format=json&key=7JHOQ1KX";
	struct write_buf wb;
	write_buf_init(&wb);
	CURLcode success = query(req, (void*) &wb);
	if(success != 0){
		printf("Error: CURL query failed\n");
		return -1;
	}
	struct ll_coord* head = get_coords_from_grid_resp_str(wb.buf);
	write_buf_cleanup(&wb);
	write_buf_init(&wb);
	struct ll_coord* p = head;
	while(p != NULL){
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
