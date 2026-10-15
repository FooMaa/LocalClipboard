#pragma once
class QQmlApplicationEngine;
class AppController;
void initializeAppUi();
void configureAppEngine(QQmlApplicationEngine &engine, AppController &controller, bool background = false);
