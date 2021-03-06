#include <stdint.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <fstream>
#include <iostream>

#include "util/os.h"
#include "bits/config.h"
#include "eveapi.h"

EveApiFetcher::~EveApiFetcher (void)
{
  this->conn_sigdone.disconnect();
}

/* ---------------------------------------------------------------- */

AsyncHttp*
EveApiFetcher::setup_fetcher (void)
{
  /* Setup HTTP fetcher. */
  AsyncHttp* fetcher = AsyncHttp::create();
  Config::setup_http(fetcher, true);
  fetcher->set_host("api.eveonline.com");

  /* Setup HTTP post data. */
  std::string post_data;
  post_data += this->auth.is_apiv1 ? "userID=" : "keyID=";
  post_data += this->auth.user_id;
  post_data += this->auth.is_apiv1 ? "&apiKey=" : "&vCode=";
  post_data += this->auth.api_key;
  if (!auth.char_id.empty())
  {
    post_data += "&characterID=";
    post_data += this->auth.char_id;
  }
  fetcher->set_data(HTTP_METHOD_POST, post_data);

  switch (this->type)
  {
    case API_DOCTYPE_CHARLIST:
      fetcher->set_path("/account/Characters.xml.aspx");
      break;
    case API_DOCTYPE_CHARSHEET:
      fetcher->set_path("/char/CharacterSheet.xml.aspx");
      break;
    case API_DOCTYPE_INTRAINING:
      fetcher->set_path("/char/SkillInTraining.xml.aspx");
      break;
    case API_DOCTYPE_SKILLQUEUE:
      fetcher->set_path("/char/SkillQueue.xml.aspx");
      break;
    default:
      delete fetcher;
      std::cout << "Bug: Invalid API document type" << std::endl;
      return 0;
  }

  return fetcher;
}

/* ---------------------------------------------------------------- */

void
EveApiFetcher::request (void)
{
  AsyncHttp* fetcher = this->setup_fetcher();
  if (fetcher == 0)
    return;

  EveApiData ret;

  this->busy = true;

  try
  {
    HttpDataPtr data = fetcher->request();
    ret.data = data;
  }
  catch (Exception& e)
  {
    ret.data.reset();
    ret.exception = e;
  }

  delete fetcher;

  this->busy = false;

  this->process_caching(ret);
  this->sig_done.emit(ret);
}

/* ---------------------------------------------------------------- */

void
EveApiFetcher::async_request (void)
{
  std::cout << "Request XML: " << this->get_doc_name() << " ..." << std::endl;

  AsyncHttp* fetcher = this->setup_fetcher();
  if (fetcher == 0)
    return;

  this->busy = true;

  this->conn_sigdone = fetcher->signal_done().connect(sigc::mem_fun
      (*this, &EveApiFetcher::async_reply));
  fetcher->async_request();
}

/* ---------------------------------------------------------------- */

void
EveApiFetcher::async_reply (AsyncHttpData data)
{
  this->busy = false;
  EveApiData apidata(data);
  this->process_caching(apidata);
  this->sig_done.emit(apidata);
}

/* ---------------------------------------------------------------- */

void
EveApiFetcher::process_caching (EveApiData& data)
{
  /* Generate filename to use as cache. */
  std::string xmlname = this->get_doc_name();
  std::string path = Config::get_conf_dir();
  path += "/sheets";
  std::string file = path;
  file += "/";
  switch (this->type)
  {
    case API_DOCTYPE_CHARLIST:
      file += this->auth.user_id;
      break;
    case API_DOCTYPE_SKILLQUEUE:
    case API_DOCTYPE_INTRAINING:
    case API_DOCTYPE_CHARSHEET:
      file += this->auth.char_id;
      break;
    default:
      std::cout << "Error: Invalid API document type!" << std::endl;
      return;
  }
  file += "_";
  file += xmlname;

  if (!data.exception.empty())
    std::cout << "Warning: " << data.exception << std::endl;

  if (data.data.get() != 0 && data.data->http_code == 200)
  {
    /* Cache successful request to file. */
    //std::cout << "Should cache to file: " << file << std::endl;
    bool dir_exists = OS::dir_exists(path.c_str());
    if (!dir_exists)
    {
      int ret = OS::mkdir(path.c_str());
      if (ret < 0)
      {
        std::cout << "Error: Couldn't create the cache directory: "
            << ::strerror(errno) << std::endl;
        return;
      }
    }

    /* Write the file. */
    std::ofstream out(file.c_str());
    if (out.fail())
    {
      std::cout << "Error: Couldn't write to cache file!" << std::endl;
      return;
    }
    std::cout << "Caching XML: " << xmlname << " ..." << std::endl;
    out.write(&data.data->data[0], data.data->data.size());
    out.close();
  }
  else
  {
    /* Read unsuccessful requests from cache if available. */
    //std::cout << "Should read from cache: " << file << std::endl;
    bool file_exists = OS::file_exists(file.c_str());
    if (!file_exists)
    {
      std::cout << "Warning: No cache file for " << xmlname << std::endl;
      return;
    }

    /* Read from file. */
    std::string input;
    std::ifstream in(file.c_str());
    while (!in.eof())
    {
      std::string line;
      std::getline(in, line);
      input += line;
    }
    in.close();

    data.data = HttpData::create();
    data.data->data.resize(input.size() + 1);
    data.locally_cached = true;
    ::memcpy(&data.data->data[0], input.c_str(), input.size() + 1);

    std::cout << "Warning: Using " << xmlname << " from cache!" << std::endl;
  }
}

/* ---------------------------------------------------------------- */

char const*
EveApiFetcher::get_doc_name (void)
{
  switch (this->type)
  {
    case API_DOCTYPE_CHARLIST:
      return "Characters.xml";
    case API_DOCTYPE_INTRAINING:
      return "SkillInTraining.xml";
    case API_DOCTYPE_CHARSHEET:
      return "CharacterSheet.xml";
    case API_DOCTYPE_SKILLQUEUE:
      return "SkillQueue.xml";
    default:
      return "Unknown";
  }
}
