#include <iostream>
#include <fstream>

#include "modules/backlight.hpp"

#include "drawtypes/label.hpp"
#include "drawtypes/progressbar.hpp"
#include "drawtypes/ramp.hpp"
#include "utils/file.hpp"

#include "modules/meta/base.inl"

POLYBAR_NS

namespace modules {
  template class module<backlight_module>;

  void backlight_module::brightness_handle::filepath(const string& path) {
    if (!file_util::exists(path)) {
      throw module_error("The file '" + path + "' does not exist");
    }
    m_path = path;
  }

  float backlight_module::brightness_handle::read() const {
    return std::strtof(file_util::contents(m_path).c_str(), nullptr);
  }

  void backlight_module::brightness_handle::write(double val) const {
      std::ofstream brightness(m_path, std::ios::out);

      if (brightness.is_open()) {
          brightness << int(val) << "\n";
          brightness.close();
      }
  }

  backlight_module::backlight_module(const bar_settings& bar, string name_)
      : inotify_module<backlight_module>(bar, move(name_)) {
    auto card = m_conf.get(name(), "card");

    // Add formats and elements
    m_formatter->add(DEFAULT_FORMAT, TAG_LABEL, {TAG_LABEL, TAG_BAR, TAG_RAMP});

    if (m_formatter->has(TAG_LABEL)) {
      m_label = load_optional_label(m_conf, name(), TAG_LABEL, "%percentage%%");
    }
    if (m_formatter->has(TAG_BAR)) {
      m_progressbar = load_progressbar(m_bar, m_conf, name(), TAG_BAR);
    }
    if (m_formatter->has(TAG_RAMP)) {
      m_ramp = load_ramp(m_conf, name(), TAG_RAMP);
    }

    // Build path to the file where the current/maximum brightness value is located
    m_val.filepath(string_util::replace(PATH_BACKLIGHT_VAL, "%card%", card));
    m_max.filepath(string_util::replace(PATH_BACKLIGHT_MAX, "%card%", card));

    m_max_val = m_max.read();

    // Add inotify watch
    watch(string_util::replace(PATH_BACKLIGHT_VAL, "%card%", card));
  }

  void backlight_module::idle() {
    sleep(75ms);
  }

  bool backlight_module::on_event(inotify_event* event) {
    if (event != nullptr) {
      m_log.trace("%s: %s", name(), event->filename);
    }

    m_percentage = static_cast<int>(m_val.read() / m_max.read() * 100.0f + 0.5f);

    if (m_label) {
      m_label->reset_tokens();
      m_label->replace_token("%percentage%", to_string(m_percentage));
    }

    return true;
  }

  /**
   * Generate the module output
   */
  string backlight_module::get_output() {
    // Get the module output early so that
    // the format prefix/suffix also gets wrapped
    // with the cmd handlers
    string output{module::get_output()};

    m_builder->cmd(mousebtn::SCROLL_UP, EVENT_SCROLLUP);
    m_builder->cmd(mousebtn::SCROLL_DOWN, EVENT_SCROLLDOWN);

    m_builder->append(output);

    m_builder->cmd_close();
    m_builder->cmd_close();

    return m_builder->flush();
  }

  bool backlight_module::build(builder* builder, const string& tag) const {
    if (tag == TAG_BAR) {
      builder->node(m_progressbar->output(m_percentage));
    } else if (tag == TAG_RAMP) {
      builder->node(m_ramp->get_by_percentage(m_percentage));
    } else if (tag == TAG_LABEL) {
      builder->node(m_label);
    } else {
      return false;
    }
    return true;
  }

  bool backlight_module::input(string&& cmd) {
    double cur_val;
    double value_mod{0.0};

    if (cmd == EVENT_SCROLLUP) {
      value_mod = 50.0;
      m_log.info("%s: Increasing value by %i%", name(), value_mod);
    } else if (cmd == EVENT_SCROLLDOWN) {
      value_mod = -50.0;
      m_log.info("%s: Decreasing value by %i%", name(), -value_mod);
    } else {
      return false;
    }

    cur_val = m_val.read() + value_mod;
    if (cur_val < 0) {
        cur_val = 0.0;
    } else if( cur_val > m_max_val) {
        cur_val = m_max_val;
    }
    m_val.write(cur_val);


    return true;
  }

}

POLYBAR_NS_END
