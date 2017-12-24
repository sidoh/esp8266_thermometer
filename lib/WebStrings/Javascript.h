#include <stddef.h>
#include <pgmspace.h>

#ifndef _JAVASCRIPT_N
#define _JAVASCRIPT_N

const char JAVASCRIPT[] PROGMEM = R""""(
(function($) {

  var SETTING_KEYS = [
    "mqtt.server",
    "mqtt.port",
    "mqtt.topic_prefix",
    "mqtt.username",
    "mqtt.password",
  
    "http.gateway_server",
    "http.hmac_secret",
  
    "admin.flag_server",
    "admin.flag_server_port",
    "admin.username",
    "admin.password",
    "admin.operating_mode",
    "admin.web_ui_port",

    "thermometers.update_interval",
    "thermometers.poll_interval",
    "thermometers.sensor_bus_pin"
  ];

  var RADIO_FIELDS = {
    "admin.operating_mode": ["always_on", "deep_sleep"]
  };

  var PASSWORD_FIELDS = {
    "admin.password": 1,
    "mqtt.password": 1
  };

  var currentSettings = {};

  var titleize = function(s) {
    var words = s.split(/[ _]+/);
    var r = [];

    words.forEach(function(word) {
      r.push(word.substr(0, 1).toUpperCase() + word.substr(1));
    });

    return r.join(' ');
  };

  var serializeForm = function(f) {
    return $(f).serializeArray()
      .reduce(
        function(a, x) { 
          var hashMatch = x.name.match(/([^\[]+)\[([^\]]+)\]/);
          if (hashMatch) {
            var key = hashMatch[1], subKey = hashMatch[2];

            if (!a[key]) {
              a[key] = {};
            } 

            a[key][subKey] = x.value;
          } else if (!a[x.name]) { 
            a[x.name] = x.value; 
          } else { 
            a[x.name] = [a[x.name]]; 
            a[x.name].push(x.value); 
          } 
          return a; 
        }, 
        {}
      );
  }

  var renderTextField = function(setting) {
    var type = PASSWORD_FIELDS[setting.key] ? 'password' : 'text';

    var elmt = '<label class="control-label" for="' + name + '">' + setting.title + '</label>';
    elmt += '<input class="form-control" id="' + name + '" name="' + setting.key + '" type="' + type + '" />';
    return elmt;
  };

  var renderRadioField = function(setting) {
    var elmt = '<label class="control-label" for="' + name + '">' + setting.title + '</label>';
    elmt += '<div class="btn-group btn-radio" data-toggle="buttons">';

    RADIO_FIELDS[setting.key].forEach(function(option) {
      elmt += '<label class="btn btn-primary">';
      elmt += '<input type="radio" name="' + setting.key + '" value="' + option + '" />';
      elmt += titleize(option);
      elmt += '</label>';
    });

    elmt += '</div>';

    return elmt;
  };

  var showError = function(e) {
    console.log(e);
    var elmt = '<div class="alert alert-danger alert-banner">Encountered an error</div>';
    $('body').prepend(elmt);
  };

  var applyThermometers = function(data) {
    if (data.length == 0) {
      $('#aliases-form fieldset').append('<i>No thermometers detected.  Try checking connections and restarting</i>');
    } else {
      var insertElmt = $('.category-title[data-category="http"] + fieldset > input:last');
      var pathsContainer = $('<div><h4>Paths</h4></div>');
      pathsContainer.insertAfter(insertElmt);

      var temperatureContainer = $('<div></div>');
      $('#current-temperatures').append(temperatureContainer);

      data.forEach(function(thermometer) {
        // Render alias field
        var elmt = $(renderTextField({name: thermometer.id, title: thermometer.id, key: thermometer.id}));
        $(elmt).val(thermometer.name);
        $('#aliases-form fieldset').append(elmt);

        // Render HTTP gateway path field
        var title = thermometer.id;
        if (thermometer.name && thermometer.name.length > 0) {
          title += ' (' + thermometer.name + ')';
        }

        elmt = $(renderTextField({name: thermometer.id, title: title, key: 'http.sensor_paths[' + thermometer.id + ']'}));
        elmt.val(currentSettings['http.sensor_paths'][thermometer.id]);
        pathsContainer.append(elmt);

        elmt = $(renderTextField({name: thermometer.id, title: title, key: thermometer.id}));
        elmt.val(thermometer.temperature);
        elmt.prop('disabled', true);
        temperatureContainer.append(elmt);
      });
    }
  };

  var loadThermometers = function() {
    $.ajax({
      url: '/thermometers',
      dataType: 'json',
      success: applyThermometers,
      error: showError
    });
  };

  var saveThermometers = function(e) {
    e.preventDefault();

    var data = serializeForm($('#aliases-form'));
    data = {"thermometers.aliases":data};

    $.ajax({
      url: '/settings',
      method: 'PUT',
      contentType: 'json',
      data: JSON.stringify(data)
    });
  };

  var applySettings = function(data) {
    currentSettings = data;

    Object.keys(data).forEach(function(key) {
      var value = data[key];

      if (RADIO_FIELDS[key]) {
        $('input[name="' + key + '"][value="' + value + '"]').click();
      } else {
        $('input[name="' + key + '"]').val(value);
      }
    });
  };

  var loadSettings = function() {
    $.ajax({
      url: '/settings',
      dataType: 'json',
      success: function(data) { applySettings(data); loadThermometers(); },
      error: showError
    });
  };

  var saveSettings = function(e) {
    e.preventDefault();

    var data = serializeForm($('#settings-form'));
    
    $.ajax({
      url: '/settings',
      method: 'PUT',
      contentType: 'application/json',
      data: JSON.stringify(data),
      error: showError
    });
  };

  var sendCommand = function(e) {
    e.preventDefault();

    var data = serializeForm(this);

    $.ajax({
      url: '/commands',
      method: 'POST',
      contentType: 'application/json',
      data: JSON.stringify(data),
      error: showError
    });
  };

  $(function() {
    var keysByCategory = {};

    //
    // Prepare settings.  Break up by category
    //
    SETTING_KEYS.forEach(function(x) {
      var keys = x.split('.');
      var category = keys[0];
      var setting = keys[1];

      if (! keysByCategory[category]) {
        keysByCategory[category] = [];
      }

      keysByCategory[category].push({key: x, name: setting, title: titleize(setting)});
    });

    //
    // Build settings form
    //
    Object.keys(keysByCategory).forEach(function(category) {
      var elmt = '<h3 class="category-title" data-category="' + category + '">' + titleize(category) + '</h3>';
      elmt += '<fieldset class="form-group">';
      
      keysByCategory[category].forEach(function(setting) {
        if (RADIO_FIELDS[setting.key]) {
          elmt += renderRadioField(setting);
        } else {
          elmt += renderTextField(setting);
        }
      });

      elmt += '</fieldset>';

      $('#settings-form').prepend(elmt);
    });

    //
    // Build aliases form
    //


    //
    // On-Load setup
    //
    $('#settings-form btn-radio').button('toggle');
    $('#settings-form').submit(saveSettings);
    $('#aliases-form').submit(saveThermometers);
    $('.command-form').submit(sendCommand);

    loadSettings();
  });

})(jQuery);
)"""";

#endif