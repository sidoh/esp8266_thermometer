#include <stddef.h>
#include <pgmspace.h>

#ifndef _INDEX_PAGE_H
#define _INDEX_PAGE_H

const char INDEX_PAGE_HTML[] PROGMEM = R""""(
<!doctype html>

<html lang="en">
<head>
  <meta charset="utf-8">

  <title>Thermometer</title>

  <link rel="stylesheet" href="https://bootswatch.com/3/cosmo/bootstrap.min.css"/>
  <link rel="stylesheet" href="/style.css"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <!--[if lt IE 9]>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/html5shiv/3.7.3/html5shiv.js"></script>
  <![endif]-->
</head>

<body>
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.1.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script>
  <script src="https://gitcdn.github.io/bootstrap-toggle/2.2.2/js/bootstrap-toggle.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/underscore.js/1.8.3/underscore-min.js"></script>
  <script src="/script.js"></script>

  <div class="navbar navbar-default navbar-fixed-top">
    <div class="container">
      <div class="navbar-header">
        <a href="../" class="navbar-brand">Thermometer</a>
      </div>
      <div class="navbar-collapse collapse" id="navbar-main">

      </div>
    </div>
  </div>

  <div class="container">
    <div class="page-header" id="banner">
      <div class="row">
        <div class="col-lg-8 col-md-7 col-sm-6">
          <h1>Nav</h1>
        </div>
      </div>
      <div class="row">
        <div class="col-lg-3 col-md-3 col-sm-4">
          <div class="list-group table-of-contents">
            <a class="list-group-item" href="#configure">Configure</a>
            <a class="list-group-item" href="#temperatures">Temperatures</a>
            <a class="list-group-item" href="#admin">Admin</a>
          </div>
        </div>
      </div>
    </div>

    <div class="thermometer-section clearfix" id="configure">
      <div class="row">
        <div class="col-lg-12">
          <div class="page-header">
            <h1>Configure</h1>
          </div>

          <div class="thermometer-section-body">
            <div class="col-lg-6">
              <div class="well">
                <form action="/settings" method="put" id="settings-form">

                  <button type="submit" class="btn btn-primary">Submit</button>
                </form>
              </div>
            </div>

            <div class="col-lg-6">
              <div class="well">
                <form action="/settings" method="put" id="aliases-form">
                  <h3 class="category-title">Device Aliases</h3>
                  <fieldset class="form-group">

                  </fieldset>

                  <button type="submit" class="btn btn-primary">Submit</button>
                </form>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="thermometer-section clearfix" id="temperatures">
      <div class="row">
        <div class="col-lg-12">
          <div class="page-header">
            <h1 id="Temperatures">Temperatures</h1>
          </div>
        </div>
      </div>

      <div class="row">
        <div class="col-lg-6">
          <div class="well" id="current-temperatures"></div>
        </div>
      </div>
    </div>

    <div class="thermometer-section clearfix" id="admin">
      <div class="row">
        <div class="col-lg-12">
          <div class="page-header">
            <h1 id="Temperatures">Admin</h1>
          </div>
        </div>
      </div>

      <div class="row">
        <div class="col-lg-6">
          <div class="well">
            <h3 class="category-title">Firmware Upgrade</h3>

            <div class="alert alert-info">
              <p>
                <b>Make sure the binary you're uploading was compiled for your board!</b> 
                Firmware with incompatible settings could prevent boots. If this happens, reflash the board with USB.
              </p>
            </div>

            <form action="/firmware" method="post">
              <fieldset class="form-group">
                <input type="file" class="form-control" name="file" />
              </fieldset>

              <input type="submit" name="submit" class="btn btn-danger" />
            </form>
          </div>
        </div>

        <div class="col-lg-6">
          <div class="well">
            <h3 class="category-title">Admin Actions</h3>

            <form action="/commands" method="post" class="command-form">
              <input type="hidden" name="command" value="reboot" />
              <input type="submit" name="submit" class="btn btn-danger" value="Reboot" />
            </form>
          </div>
        </div>
      </div>
    </div>
  </div>
</body>
</html>
)"""";

#endif