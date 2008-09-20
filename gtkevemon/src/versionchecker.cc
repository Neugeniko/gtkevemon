#include <iostream>
#include <glibmm.h>
#include <gtkmm/messagedialog.h>

#include "config.h"
#include "defines.h"
#include "versionchecker.h"

VersionChecker::VersionChecker (void)
{
  this->info_display = 0;
  this->parent_window = 0;
  Glib::signal_timeout().connect(sigc::mem_fun
      (*this, &VersionChecker::request_version), VERSION_CHECK_INTERVAL);
}

/* ---------------------------------------------------------------- */

VersionChecker::~VersionChecker (void)
{
  this->request_conn.disconnect();
}

/* ---------------------------------------------------------------- */

bool
VersionChecker::request_version (void)
{
  ConfValuePtr check = Config::conf.get_value("versionchecker.enabled");
  if (!check->get_bool())
    return true;

  AsyncHttp* http = AsyncHttp::create();
  http->set_host("gtkevemon.battleclinic.com");
  http->set_path("/svn_version.txt");
  http->set_agent("GtkEveMon");
  this->request_conn = http->signal_done().connect(sigc::mem_fun
      (*this, &VersionChecker::handle_result));
  http->async_request();

  return true;
}

/* ---------------------------------------------------------------- */

void
VersionChecker::handle_result (AsyncHttpData result)
{
  if (result.data.get() == 0)
  {
    std::cout << "Unable to perform version check: "
        << result.exception << std::endl;
    return;
  }

  Glib::ustring cur_version(GTKEVEMON_VERSION_STR);
  Glib::ustring svn_version(result.data->data);

#define INVALID_RESPONSE "Version checker received an invalid response!"

  /* Make some sanity checks. */
  if (svn_version.empty())
  {
    this->info_display->append(INFO_WARNING, INVALID_RESPONSE,
        "The version checker received a zero-length string.");
    return;
  }

  if (svn_version.size() > 64)
  {
    this->info_display->append(INFO_WARNING, INVALID_RESPONSE,
        "The version checker received a too long string.");
    return;
  }

  size_t newline_pos = svn_version.find_first_of('\n');
  if (newline_pos != std::string::npos)
    svn_version = svn_version.substr(0, newline_pos);

  /* Check the version. */
  if (svn_version == cur_version)
    return;

  ConfValuePtr last_seen = Config::conf.get_value("versionchecker.last_seen");
  if (svn_version == **last_seen)
    return;

  /* Mark current version as seen. Inform user about the new version. */
  last_seen->set(svn_version);
  Config::save_to_file();

  if (this->info_display != 0)
  {
    this->info_display->append(INFO_NOTIFICATION,
        "The current SVN version is " + svn_version,
        "A new version of GtkEveMon is available "
        "in SVN! Please consider updating to the latest version.\n\n"
        "Your local version is: " + cur_version + "\n"
        "The latest version is: " + svn_version + "\n\n"
        "Take a look at the forums for further information:\n"
        "http://www.battleclinic.com/forum/index.php#43");
  }
  else
  {
    Gtk::MessageDialog md("There is an update available!",
        false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK);
    md.set_secondary_text("A new version of GtkEveMon is available "
        "in SVN! Please consider updating to the latest version.\n\n"
        "Your local version is: " + cur_version + "\n"
        "The latest version is: " + svn_version + "\n\n"
        "Take a look at the forums for further information:\n"
        "http://www.battleclinic.com/forum/index.php#43");
    md.set_title("Version check - GtkEveMon");
    if (this->parent_window != 0)
      md.set_transient_for(*this->parent_window);
    md.run();
  }
}