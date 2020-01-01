# Release History

## 1.0.0 (2020-01-01)
* Public release.
* Moved RoutingBuffer to a new project.

## 0.4.0 (2019-06-19)
* No longer supports tls.

## 0.3.1 (2019-06-19)
* Updated statistics to be lock-free.

## 0.3.0 (2019-06-19)
* Updated statistics to be more accurate

## 0.2.2 (2019-06-05)
* Changed default logger to rotating file logger.
* Changed default regex library to use Boost.
* fixed bug with statistics.
* fixed bug when app was started more than 1.

## 0.2.1 (2019-06-04)
Now can specify logging directory.

## 0.2.0 (2019-06-04)
* Code clean.
* Docker support.
* Delete data in redis if the data is in wrong format.
* Provide healthcheck (/healthcheck).
* Provide statistics (/stats).
* Auto Reconnection logic

## 0.1.6 (2019-05-13)
* Now it caches non-RCS user results.
* Notification system is needed to apply non-RCS user caching.
* Added TTL. 7 days for both RCS and non-RCS user.

## 0.1.5 (2019-05-02)
* Added unit tests and fixed some bugs.
* sync_command was changed to command_sync.

## 0.1.4 (2019-04-25)
* Added logging and fixed some bugs.

## 0.1.3 (2019-04-18)
* Added soap parser and Boost.bease based redis buffer.

## 0.1.2 (2019-04-01)
* Added redis buffer example.

## 0.1.1 (2019-02-27)
First working version of redicon.

## 0.1.0 (2019-02-22)
Initial release.
