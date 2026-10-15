#pragma once
class QApplication;
class QQmlApplicationEngine;
class MobileController;
void configureMobileEngine(QQmlApplicationEngine &engine, MobileController &controller);
int runMobileApp(QApplication &app);
