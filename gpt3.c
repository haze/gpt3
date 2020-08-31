// TODO(haze): selectable engine

#include <curl/curl.h>
#include <curl/easy.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPT_KEY_STR "GPT3_KEY"

typedef struct gpt_client {
  const char *const api_key;
  const char *const context;
} gpt_client;

struct growable_string {
  char *ptr;
  size_t length;
};

// https://stackoverflow.com/a/2329792
void init_string(struct growable_string *s) {
  s->length = 0;
  s->ptr = malloc(s->length + 1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed when initializing growable string\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

void destory_string(struct growable_string *s) { free(s->ptr); }

typedef struct conversation {
  struct growable_string buffer;
} conversation;

void init_conversation(conversation *conv) { init_string(&conv->buffer); }
void convo_add_human_line(conversation *conv, char *text) {}
void convo_add_ai_line(conversation *conv, const char *text) {}
void destory_convo(conversation *convo) { destory_string(&convo->buffer); }

size_t curl_growable_string_write_func(void *ptr, size_t size, size_t nmemb,
                                       struct growable_string *s) {
  size_t new_len = s->length + size * nmemb;
  s->ptr = realloc(s->ptr, new_len + 1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr + s->length, ptr, size * nmemb);
  s->ptr[new_len] = '\0';
  s->length = new_len;

  return size * nmemb;
}

// gpt_begin_session will read input from stdin, and
static void gpt_begin_session(gpt_client *client, char **err) {
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if (!curl) {
    *err = "Failed to initialize cURL";
    return;
  }

  struct curl_slist *headers = NULL;

  char authorization_header[128];
  sprintf(authorization_header, "Authorization: Bearer %s", client->api_key);

  headers = curl_slist_append(headers, authorization_header);
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers,
                              "Content-Type: application/json; charset=utf-8");

  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://api.openai.com/v1/engines/davinci/completions");
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                   curl_growable_string_write_func);

  conversation convo;
  init_conversation(&convo);

  char *line = NULL;
  size_t len = 0;
  ssize_t bytes_read = 0;
  // first try new string every iteration, then try to reuse the string
  while ((bytes_read = getline(&line, &len, stdin)) != -1) {
    struct growable_string data;
    init_string(&data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    /* printf("(%zu bytes): %s", bytes_read, line); */
    convo_add_human_line(&convo, line);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      *err = "curl_easy_perform() failed";
      return;
    }
    struct json_object *response_object, *choices_array, *first_choice,
        *response_text;
    response_object = json_tokener_parse(data.ptr);
    // get only the first choice
    choices_array = json_object_object_get(response_object, "choices");
    first_choice = json_object_array_get_idx(choices_array, 0);
    response_text = json_object_object_get(first_choice, "text");
    convo_add_ai_line(&convo, json_object_get_string(response_text));
    json_object_put(response_object);
  }

  destory_convo(&convo);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(line);
}

int main(int argc, char **argv) {
  char const *const gpt3_key = getenv(GPT_KEY_STR);
  if (!gpt3_key) {
    fprintf(stderr, "Missing '%s' in environment variables\n", GPT_KEY_STR);
    return EXIT_FAILURE;
  }

  gpt_client client = {.api_key = gpt3_key};
  char *err_reason = NULL;
  gpt_begin_session(&client, &err_reason);

  if (err_reason) {
    fprintf(stderr, "gpt session failure: %s\n", err_reason);
    return EXIT_FAILURE;
  }

  printf("bye!\n");
  return EXIT_SUCCESS;
}
