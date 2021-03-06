/**
 * Copyright (c) 2015 SopraSteria
 * All rights reserved.
 * Author: Emmanuel Benazera <emmanuel.benazera@deepdetect.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of SopraSteria nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SOPRASTERIA ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SOPRASTERIA BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "job.h"
#include <gtest/gtest.h>

using namespace miw;

std::string path = "../";

TEST(job,proxy_format)
{
  job j;
  std::string arg_line = "-fnames ../data/pxyinternet-10lines.log.orig.anon -format_name ../miw/formats/proxy_format -output_format mem";
  std::vector<std::string> args;
  log_format::tokenize(arg_line,-1,args," ","");
  char* cargs[args.size()+1];
  cargs[0] = "miw";
  for (size_t i=0;i<args.size();i++)
    cargs[i+1] = const_cast<char*>(args.at(i).c_str());
  j.execute(args.size()+1,cargs);
  ASSERT_EQ(2,j._results->size());
}

TEST(job,domain_controller_format)
{
  job j;
  std::string arg_line = "-fnames ../data/domain_controller_100lines_test.log -format_name ../miw/formats/domain_controller_format -output_format mem --skip_header";
  std::vector<std::string> args;
  log_format::tokenize(arg_line,-1,args," ","");
  char* cargs[args.size()+1];
  cargs[0] = "miw";
  for (size_t i=0;i<args.size();i++)
    cargs[i+1] = const_cast<char*>(args.at(i).c_str());
  j.execute(args.size()+1,cargs);
  ASSERT_EQ(4,j._results->size());
}

TEST(job,evtx)
{
  job j;
  std::string arg_line = "-fnames ../data/SecuritySample_10.csv -format_name ../miw/formats/evtx -output_format mem";
  std::vector<std::string> args;
  log_format::tokenize(arg_line,-1,args," ","");
  char* cargs[args.size()+1];
  cargs[0] = "miw";
  for (size_t i=0;i<args.size();i++)
    cargs[i+1] = const_cast<char*>(args.at(i).c_str());
  j.execute(args.size()+1,cargs);
  ASSERT_EQ(1,j._results->size());
  //TODO: check on per field result
}

TEST(job,evtx2)
{
  job j;
  std::string arg_line = "-fnames ../data/SecuritySample_10_2.csv -format_name ../miw/formats/evtx2 -output_format mem";
  std::vector<std::string> args;
  log_format::tokenize(arg_line,-1,args," ","");
  char* cargs[args.size()+1];
  cargs[0] = "miw";
  for (size_t i=0;i<args.size();i++)
    cargs[i+1] = const_cast<char*>(args.at(i).c_str());
  j.execute(args.size()+1,cargs);
  ASSERT_EQ(1,j._results->size());
  //TODO: check on per field result
}

/*TEST(job,firewall_checkpoint)
{
  job j;
  std::string arg_line = "-fnames ../data/fw_checkpoint_100lines.log -format_name ../miw/formats/firewall_checkpoint -output_format mem";
  std::vector<std::string> args;
  log_format::tokenize(arg_line,-1,args," ","");
  char* cargs[args.size()+1];
  cargs[0] = "miw";
  for (size_t i=0;i<args.size();i++)
    cargs[i+1] = const_cast<char*>(args.at(i).c_str());
  j.execute(args.size()+1,cargs);
  ASSERT_EQ(1,j._results->size());
  }*/

TEST(job,allCiscoIportwsa)
{
  job j;
  std::string arg_line = "-fnames ../data/RSSallCisco10.csv -format_name ../miw/formats/allCiscoIportwsa -output_format mem";
  std::vector<std::string> args;
  log_format::tokenize(arg_line,-1,args," ","");
  char* cargs[args.size()+1];
  cargs[0] = "miw";
  for (size_t i=0;i<args.size();i++)
    cargs[i+1] = const_cast<char*>(args.at(i).c_str());
  j.execute(args.size()+1,cargs);
  ASSERT_EQ(7,j._results->size());
  //TODO: check on per field result
}
