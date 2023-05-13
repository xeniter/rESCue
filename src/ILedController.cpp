#include "config.h"
#include "ILedController.h"
#include "AppConfiguration.h"
#include <Logger.h>

int idleTimer = 0;

void ILedController::loop(const int *new_forward, const int *new_backward, const int *new_idle, const int *new_brake) {

    if(!AppConfiguration::getInstance()->config.lightsSwitch) {
        this->changePattern(Pattern::NONE, *(new_forward) == HIGH, false);
    }
    // is motor brake active?
    if (*(new_brake) == HIGH) {
        // flash backlights
        this->changePattern(Pattern::RESCUE_FLASH_LIGHT, *(new_forward) == HIGH, false);
    }

    this->update();

    // is there a change detected
    if (old_forward != *(new_forward) || old_backward != *(new_backward)) {
        if (Logger::getLogLevel() == Logger::VERBOSE) {
            char buf[128];
            snprintf(buf, 128, "change detected: forward is %d was %d, backward is %d was %d",
                     *(new_forward), old_forward, *(new_backward), old_backward);
            Logger::verbose(LOG_TAG_LED, buf);
        }

        this->changePattern(Pattern::FADE, (*new_forward) == HIGH, false);

        old_forward = *(new_forward);
        old_backward = *(new_backward);
    }

    //idle state???
    if (old_idle != *(new_idle)) {
        if (*(new_idle) == HIGH) {
            if (idleTimer == 0) {
                idleTimer = millis();
            }
            this->idleSequence();
        }
        old_idle = *(new_idle);
    }

    if (idleTimer != 0 && AppConfiguration::getInstance()->config.idleLightTimeout > 0 &&
      millis() - idleTimer > AppConfiguration::getInstance()->config.idleLightTimeout) {
        Logger::notice(LOG_TAG_LED, "Turn off idle light");
        this->changePattern(NONE, true, false);
        idleTimer = 0;
    }
}


