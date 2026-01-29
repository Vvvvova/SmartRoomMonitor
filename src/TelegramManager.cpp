#include "TelegramManager.h"

TelegramManager::TelegramManager() 
    : lastPollTime(0), lastAdviceCode(-1), 
      lastClimateState(ClimateState::STABLE), 
      moldAlertSent(false), timeoutAlertSent(false) {
    // Insecure client for simplicity (no cert management)
    client.setInsecure();
    bot = new UniversalTelegramBot(BOT_TOKEN, client);
}

void TelegramManager::begin() {
    // Add Owner as first subscriber
    subscribers.push_back({OWNER_CHAT_ID, false, "Admin", 0});
    
    // Send Hello to Owner
    bot->sendMessage(OWNER_CHAT_ID, "🤖 **Climate Bot Online**\nSystem restarted.", "Markdown");
    sendMainMenu(OWNER_CHAT_ID);
}

void TelegramManager::update() {
    // FIX: Skip Telegram operations if WiFi is not connected
    // This prevents long timeouts (10+ seconds) when network is down
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    // 1. Poll Telegram (3s interval - balance between responsiveness and WiFi load)
    if (millis() - lastPollTime > 3000) {
        int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        while (numNewMessages) {
            handleNewMessages(numNewMessages);
            numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        }
        lastPollTime = millis();
    }

    // 2. Check for Alerts (Logic: Change of State)
    // Read from CoreState (safe snapshot)
    CoreSnapshot sn = g_state.getSnapshot();
    ClimateState currentState = sn.state;
    unsigned long stateEnterTime = sn.stateEnterTime;
    float temp = sn.temp;
    float dewPoint = sn.dewPoint;
    
    // --- STATE BASED ALERTS ---
    
    // A. Transition to TARGET_MET (Success)
    if (lastClimateState == ClimateState::VENTILATING && currentState == ClimateState::TARGET_MET) {
        String msg = "✅ **Цель достигнута!**\nВлажность в норме. Можно закрывать.";
        broadcastAlert(msg, 1);
    }

    // B. Transition to INEFFICIENT (Stalled)
    if (lastClimateState == ClimateState::VENTILATING && currentState == ClimateState::INEFFICIENT) {
        String msg = "⚠️ **Эффективность упала**\nВлага почти не уходит. Закрывайте, чтобы не выстужать стены.";
        broadcastAlert(msg, 2);
    }

    // C. Rebound (Window Closed) - Silent Log
    if (lastClimateState != ClimateState::STABLE && currentState == ClimateState::STABLE) {
        Serial.println("Telegram: Окно закрыто (Отскок влажности)");
    }
    
    // B. Timeout (Safety Timer 20m)
    if (currentState == ClimateState::VENTILATING) {
        unsigned long dur = millis() - stateEnterTime;
        if (dur > 20 * 60 * 1000 && !timeoutAlertSent) {
             broadcastAlert("⚠️ **Таймер безопасности:** 20 мин.\nРекомендуется закрыть окно во избежание переохлаждения.", 2);
             timeoutAlertSent = true;
        }
    } else {
        timeoutAlertSent = false; // Reset when not ventilating
    }
    
    // C. Mold Risk (Independent Check)
    // Condition: Temp - DP < 3.0
    float margin = temp - dewPoint;
    if (!isnan(margin) && margin < 3.0) {
        if (!moldAlertSent) {
            broadcastAlert("🔴 **Риск плесени!**\nСтены холодные. Требуется прогрев и осушение!", 2);
            moldAlertSent = true; 
        }
    } else {
        if (margin > 3.5) moldAlertSent = false; // Hysteresis 0.5C to reset
    }
    
    lastClimateState = currentState;
}

void TelegramManager::handleNewMessages(int numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
        String chatId = String(bot->messages[i].chat_id);
        String text = bot->messages[i].text;
        String from_name = bot->messages[i].from_name;

        // Auto-subscribe new users (Family Mode)
        subscribe(chatId, from_name);

        if (text == "/start") {
            sendMainMenu(chatId, "Добро пожаловать, " + from_name + "!");
        } 
        else if (text == "🌡️ Статус") {
            sendStatus(chatId);
        }
        else if (text == "🔇/🔊 Звук") {
            toggleMute(chatId);
        }
        else if (text == "🔗 Веб-панель") {
             // Send Inline Button with URL
             String ip = WiFi.localIP().toString();
             String url = "http://" + ip;
             String keyboardJson = "[[{ \"text\": \"🖥️ Открыть Панель\", \"url\": \"" + url + "\" }]]";
             bot->sendMessageWithInlineKeyboard(chatId, "Нажмите кнопку, чтобы открыть красивый дэшборд:", "", keyboardJson);
        }
        else {
             sendMainMenu(chatId);
        }
    }
}

void TelegramManager::sendMainMenu(const String& chatId, const String& welcomeMsg) {
    String keyboardJson = "[[\"🌡️ Статус\", \"🔇/🔊 Звук\"], [\"🔗 Веб-панель\"]]";
    bot->sendMessageWithReplyKeyboard(chatId, welcomeMsg.length() > 0 ? welcomeMsg : "Меню:", "", keyboardJson, true);
}

void TelegramManager::sendStatus(const String& chatId) {
    // Read from CoreState (safe snapshot)
    CoreSnapshot sn = g_state.getSnapshot();
    float t = sn.temp;
    float h = sn.hum;
    float outT = sn.outTemp;
    String advice = sn.advice;
    int code = sn.adviceCode;
    String weatherStatus = sn.weatherStatus;
    
    String icon = "😐";
    if(code == 3) icon = "✅"; // Good/Safe
    if(code == 2) icon = "🔴"; // Critical
    if(code == 1) icon = "🟡"; // Vent
    
    String msg = icon + " **КЛИМАТ:**\n\n";
    msg += "🏠 **Дома:** " + String(t, 1) + "°C | " + String(h, 1) + "%\n";
    if(!isnan(outT)) {
        msg += "🌳 **Улица:** " + String(outT, 1) + "°C\n";
    } else {
        msg += "🌑 **Погода:** " + weatherStatus + "\n";
    }
    msg += "\n💡 **Совет:** " + advice;
    
    bot->sendMessage(chatId, msg, "Markdown");
}

void TelegramManager::broadcastAlert(const String& msg, int level) {
    for (auto &sub : subscribers) {
        if (!sub.isMuted) {
            // Optional: Add silent flag for minor alerts? keeping loud for now
            bot->sendMessage(sub.chatId, msg, "Markdown");
        }
    }
}

void TelegramManager::subscribe(const String& chatId, const String& firstName) {
    for (auto &sub : subscribers) {
        if (sub.chatId == chatId) return; // Already exists
    }
    subscribers.push_back({chatId, false, firstName, 0});
    bot->sendMessage(chatId, "Вы подписаны на уведомления! 🔔", "Markdown");
}

void TelegramManager::toggleMute(const String& chatId) {
    for (auto &sub : subscribers) {
        if (sub.chatId == chatId) {
            sub.isMuted = !sub.isMuted;
            String status = sub.isMuted ? "🔇 Уведомления ОТКЛЮЧЕНЫ" : "🔔 Уведомления ВКЛЮЧЕНЫ";
            bot->sendMessage(sub.chatId, status, "Markdown");
            return;
        }
    }
}

