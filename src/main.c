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

int main(int argc, char** argv)
{
	char* req = "https://api.what3words.com/v2/grid?bbox=52.208867,0.117540,52.207988,0.116126&format=json&key=7JHOQ1KX";
	struct write_buf wb;
	write_buf_init(&wb);
	CURLcode success = query(req, (void*) &wb);
	printf("%s\n", wb.buf);
	write_buf_cleanup(&wb);
	return success;
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

static size_t write_callback(
		void* buf,  size_t sz,
		size_t nmemb, void* userp)
{
	struct write_buf* wb = (struct write_buf*) userp;
	size_t len = sz * nmemb;
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
