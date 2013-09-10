/**
 * Copyright (c) 2013 XPLR Software Inc.
 * All rights reserved.
 * Author: Emmanuel Benazera <emmanuel.benazera@xplr.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of XPLR Software Inc nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY XPLR SOFTWARE INC ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL XPLR SOFTWARE INC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Log preprocessing and compacting tool.
 */

#include "log_format.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <strings.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sched.h>
#include "application.hh"
#include "defsplitter.hh"
#include "bench.hh"
#ifdef JOS_USER
#include "wc-datafile.h"
#include <inc/sysprof.h>
#endif
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>
#include <jsoncpp/json/json.h>
#include <climits>
#include "MurmurHash3.h"

using namespace miw;

#define DEFAULT_NDISP 10
//#define DEBUG

enum { with_value_modifier = 0 };

static int alphanumeric;
static bool count_users = false;
static int time_merge = 0; // 0: merge by day, 1: merge by hour.
static bool sg = false; // temporary option to use external logs for testing purposes.
static bool hashed_keys = false;
static bool airbus = false;
static unsigned int seed = 4294967291UL; // prime.
static long skipped_logs = 0;
static log_format lf;
static std::string app_name;

/*template <class T>
static std::string to_string (const T& t)
{
  std::stringstream ss;
  ss << t;
  return ss.str();
  };*/

 /* N[0] - contains least significant bits, N[3] - most significant */
char* Bin128ToDec(const uint32_t N[4])
{
  // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
  static char s[128 / 3 + 1 + 1];
  uint32_t n[4];
  char* p = s;
  int i;

  memset(s, '0', sizeof(s) - 1);
  s[sizeof(s) - 1] = '\0';

  memcpy(n, N, sizeof(n));

  for (i = 0; i < 128; i++)
    {
      int j, carry;

      carry = (n[3] >= 0x80000000);
      // Shift n[] left, doubling it
      n[3] = ((n[3] << 1) & 0xFFFFFFFF) + (n[2] >= 0x80000000);
      n[2] = ((n[2] << 1) & 0xFFFFFFFF) + (n[1] >= 0x80000000);
      n[1] = ((n[1] << 1) & 0xFFFFFFFF) + (n[0] >= 0x80000000);
      n[0] = ((n[0] << 1) & 0xFFFFFFFF);

      // Add s[] to itself in decimal, doubling it
      for (j = sizeof(s) - 2; j >= 0; j--)
	{
	  s[j] += s[j] - '0' + carry;

	  carry = (s[j] > '9');

	  if (carry)
	    {
	      s[j] -= 10;
	    }
	}
    }

  while ((p[0] == '0') && (p < &s[sizeof(s) - 2]))
    {
      p++;
    }

  return p;
}

void parse_url_host_and_path(const std::string &url,
			     std::string &host, std::string &path)
{
  size_t p1 = 0;
  if ((p1=url.find("http://"))!=std::string::npos)
    p1 += 7;
  else if ((p1=url.find("https://"))!=std::string::npos)
    p1 += 8;
  else if ((p1=url.find("tcp://"))!=std::string::npos)
    p1 += 6;
  else if ((p1=url.find("ftp://"))!=std::string::npos)
    p1 += 6;
  else if (p1 == std::string::npos) // malformed url.
    {
      std::cerr << "malformed url: " << url << std::endl;
      host = "";
      path = "";
      return;
    }
  size_t p2 = 0;
  if ((p2 = url.find("/",p1))!=std::string::npos)
    {
      try
	{
	  host  = url.substr(p1,p2-p1);
	}
      catch (std::exception &e)
	{
	  host = "";
	}
      try
	{
	  path = url.substr(p2);
	}
      catch (std::exception &e)
	{
	  path = "";
	}
    }
  else
    {
      host = url.substr(p1);
      path = "";
    }
}

void free_records(xarray<keyval_t> *wc_vals)
{
  for (uint32_t i=0;i<wc_vals->size();i++)
    {
      log_record *lr = (log_record*)wc_vals->at(i)->val;
      delete lr;
    }
}

class bl : public map_reduce {
public:
  bl(const char *f, int nsplit) : s_(f, nsplit) {}
  bool split(split_t *ma, int ncores) {
    return s_.split(ma, ncores, "\n",0);
      }
  int key_compare(const void *s1, const void *s2) {
    return  strcasecmp((const char *) s1, (const char *) s2);
  }
  
  void map_function(split_t *ma);

  void reduce_function(void *key_in, void **vals_in, size_t vals_len);
    
    /* write back the sums */
  int combine_function(void *key_in, void **vals_in, size_t vals_len);
  
  void *modify_function(void *oldv, void *newv) {
    if (count_users)
      {
	uint64_t v = (uint64_t) oldv;
        uint64_t nv = (uint64_t) newv;
        return (void *) (v + nv);
      }
    else
      {
	log_record *lr1 = (log_record*) oldv;
	log_record *lr2 = (log_record*) newv;
	//lr1->_sum += lr2->_sum;
	lr1->merge(lr2);
	delete lr2;
	return (void*)lr1;
      }
  }
  
  void *key_copy(void *src, size_t s) {
    char *key = strdup((char*)src);
      return key;
    }
    
  void key_free(void *k)
  {
    free(k);
  }
  
  int final_output_compare(const keyval_t *kv1, const keyval_t *kv2) {
#ifdef HADOOP
    return strcmp((char *) kv1->key_, (char *) kv2->key_);
#else
    if (alphanumeric)
      return strcmp((char *) kv1->key_, (char *) kv2->key_);
    size_t i1 = (size_t) kv1->val;
    size_t i2 = (size_t) kv2->val;
    if (i1 != i2)
      return i2 - i1;
    else
      return strcmp((char *) kv1->key_, (char *) kv2->key_);
#endif
  }
  
  bool has_value_modifier() const {
    return with_value_modifier;
  }

private:
  defsplitter s_;
  };

void bl::map_function(split_t *ma)
{
  std::vector<log_record*> log_records;
  std::string dat = (char*)ma->data;
  lf.parse_data(dat,ma->length,app_name,log_records);
  
#ifdef DEBUG
  std::cout << "number of mapped records: " << log_records.size() << std::endl;
#endif
  for (size_t i=0;i<log_records.size();i++)
    {
      log_records.at(i)->_sum = 1;
      std::string key = /*hashed_keys ? log_records.at(i)->hkey() : */log_records.at(i)->key();
      const char *key_str = key.c_str();
      map_emit((void*)key_str,(void*)log_records.at(i),strlen(key_str));
    }
}

/*  void bl::map_function(split_t *ma)
  {
  std::vector<log_record*> log_records;
  log_parser::parse(ma,log_records);
#ifdef DEBUG
  std::cout << "number of mapped records: " << log_records.size() << std::endl;
#endif
  for (size_t i=0;i<log_records.size();i++)
    {
      if (count_users) // only count records per user.
	{
	  const char *u = log_records.at(i)->_username.c_str();
	  map_emit((void*)u,(void*)1,strlen(u));
	}
      else 
	{
	  log_records.at(i)->_sum = 1;
	  std::string key = hashed_keys ? log_records.at(i)->hkey() : log_records.at(i)->key();
	  const char *key_str = key.c_str();
	  map_emit((void*)key_str,(void*)log_records.at(i),strlen(key_str));
	}
    }
    }*/

int bl::combine_function(void *key_in, void **vals_in, size_t vals_len)
{
  if (!count_users)
    {
      log_record **lrecords = (log_record**)vals_in;
      for (uint32_t i=1;i<vals_len;i++)
	{
	  lrecords[0]->merge(lrecords[i]);
	  delete lrecords[i];
	}
    }
  else
    {
      long *vals = (long *) vals_in;
      for (uint32_t i = 1; i < vals_len; i++)
	vals[0] += vals[i];
    }
  return 1;
}

void bl::reduce_function(void *key_in, void **vals_in, size_t vals_len)
{
  if (count_users)
    {
      long *vals = (long*) vals_in;
      long sum = 0;
      for (uint32_t i=0;i<vals_len;i++)
	sum += vals[i];
      reduce_emit(key_in,(void*)sum);
    }
  else
    {
      log_record **lrecords = (log_record**)vals_in;
      for (uint32_t i=1;i<vals_len;i++)
	{
	  lrecords[0]->merge(lrecords[i]);
	  delete lrecords[i];
	}
      reduce_emit(key_in,(void*)lrecords[0]);
    }
}

static void print_top(xarray<keyval_t> *wc_vals, int ndisp) {
    size_t occurs = 0;
    std::multimap<long,std::string,std::greater<long> > ordered_records;
    std::multimap<long,std::string,std::greater<long> >::iterator mit;
    for (uint32_t i = 0; i < wc_vals->size(); i++)
      {
	if (count_users)
	  occurs += size_t(wc_vals->at(i)->val);
	else
	  {
	    log_record *lr = (log_record*)wc_vals->at(i)->val;
	    occurs += lr->_sum;
	    ordered_records.insert(std::pair<long,std::string>(lr->_sum,lr->key()));
	    if ((int)ordered_records.size() > ndisp)
	      {
		mit = ordered_records.end();
		mit--;
		ordered_records.erase(mit);
	      }
	  }
      }
    printf("\nlogs preprocessing: results (TOP %d from %zu keys, %zd logs):\n",
           ndisp, wc_vals->size(), occurs);
#ifdef HADOOP
    ndisp = wc_vals->size();
#else
    ndisp = std::min(ndisp, (int)wc_vals->size());
#endif
    if (count_users)
      {
	for (int i = 0; i < ndisp; i++) {
	  keyval_t *w = wc_vals->at(i);
	  printf("%15s - %d\n", (char *)w->key_, ptr2int<unsigned>(w->val));
	}
      }
    else
      {
	int c = 0;
	mit = ordered_records.begin();
	while(mit!=ordered_records.end())
	  {
	    printf("%45s - %ld\n",(*mit).second.c_str(),(*mit).first);
	    ++mit;
	    if (c++ == ndisp)
	      break;
	  }
      }
    std::cout << std::endl;
}

static void output_all(xarray<keyval_t> *wc_vals, FILE *fout) 
{
  for (uint32_t i = 0; i < wc_vals->size(); i++) 
    {
      keyval_t *w = wc_vals->at(i);
      if (count_users)
	fprintf(fout, "%18s - %d\n", (char *)w->key_,ptr2int<unsigned>(w->val));
      else fprintf(fout, "%45s - %ld\n", (char*)w->key_,static_cast<log_record*>(w->val)->_sum);
    }
}

static void output_json(xarray<keyval_t> *wc_vals, FILE *fout)
{
  Json::FastWriter writer;
  for (uint32_t i = 0; i < wc_vals->size(); i++) 
    {
      log_record *lr = (log_record*)wc_vals->at(i)->val;
      //std::string key = (const char*)wc_vals->at(i)->key_;
      Json::Value jrec = lr->to_json();
      if (fout)
	fprintf(fout,"%s",writer.write(jrec).c_str());
      else std::cout << writer.write(jrec) << std::endl;
    }
}

static void usage(char *prog) {
    printf("usage: %s <filenames> [options]\n", prog);
    printf("options:\n");
    printf("  -p #procs : # of processors to use\n");
    printf("  -m #map tasks : # of map tasks (pre-split input before MR)\n");
    printf("  -r #reduce tasks : # of reduce tasks\n");
    printf("  -l ntops : # of top val. pairs to display\n");
    printf("  -q : quiet output (for batch test)\n");
    printf("  -a : alphanumeric word count\n");
    printf("  -o filename : save output to a file\n");
    printf("  -c : count users only\n");
    printf("  -t opt : 0 merge by day, 1 merge by hour\n");
    printf("  -j : output format is JSON (for solr indexing)\n");
    printf("  -f : select log format\n");
    //printf("  -b : Airbus logs\n");
    printf("  -u : use hashed keys\n");
    printf("  -n #appname : application name for tagging records\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) 
{    
    int nprocs = 0, map_tasks = 0, ndisp = 5, reduce_tasks = 0;
    int quiet = 0;
    int json_output = 0;
    int c;
    std::string lformat_name = "default_log_format";  //TODO: replace by true default or make mandatory
    if (argc < 2)
	usage(argv[0]);
    std::vector<std::string> files;
    int i = 0;
    while (argv[++i] && argv[i][0] != '-')
      files.push_back(argv[i]);
    std::cout << "Processing " << files.size() << " file(s)\n";
    
    FILE *fout = NULL;

    while ((c = getopt(argc - 1, argv + 1, "p:l:m:r:f:n:qacjsubo:t:")) != -1) 
      {
      switch (c) {
	case 'p':
	    nprocs = atoi(optarg);
	    break;
	case 'l':
	    ndisp = atoi(optarg);
	    break;
	case 'm':
	    map_tasks = atoi(optarg);
	    break;
	case 'r':
	    reduce_tasks = atoi(optarg);
	    break;
      	case 'q':
	    quiet = 1;
	    break;
	case 'a':
	    alphanumeric = 1;
	    break;
      case 'j':
	json_output = 1;
	break;
      case 'o':
	    fout = fopen(optarg, "w+");
	    if (!fout) {
		fprintf(stderr, "unable to open %s: %s\n", optarg,
			strerror(errno));
		exit(EXIT_FAILURE);
	    }
	    break;
      case 'c':
	count_users = true;
	break;
      case 't':
	time_merge = atoi(optarg);
	break;
      case 's':
	sg = true;
	break;
      case 'u':
	hashed_keys = true;
	break;
      case 'b':
	airbus = true;
	break;
      case 'f':
	lformat_name = optarg;
	break;
      case 'n':
	app_name = optarg;
	break;
      default:
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
	    break;
	}
    }

    /* fill log format up. */
    if (lf.read(lformat_name) < 0)
      {
	std::cerr << "Error opening the log format file\n";
	exit(1);
      }
    
    /* get input file */
    for (size_t j=0;j<files.size();j++)
      {
	std::cerr << "\nProcessing file: " << files.at(j) << std::endl;
	struct stat st;
	if (stat(files.at(j).c_str(),&st)!=0)
	  {
	    std::cerr << "[Error] file not found: " << files.at(j) << std::endl;
	    continue;
	  }
	mapreduce_appbase::initialize();
	const char *fn = files.at(j).c_str();
	bl app(fn, map_tasks);
	app.set_ncore(nprocs);
	app.set_reduce_task(reduce_tasks);
	app.sched_run();
	app.print_stats();
	/* get the number of results to display */
	if (!quiet)
	  print_top(&app.results_, ndisp);
	if (fout) 
	  {
	    if (json_output)
	      output_json(&app.results_,fout);
	    else output_all(&app.results_,fout);
	  }
	else if (json_output)
	  output_json(&app.results_,NULL);
	if (!count_users)
	  free_records(&app.results_);
	app.free_results();
	mapreduce_appbase::deinitialize();
      }
    if (fout)
      fclose(fout);
    std::cout << "[Info]: skipped logs: " << skipped_logs << std::endl;
    return 0;
}
