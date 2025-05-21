function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function heat(pb_status) {
    if (pb_status == true) {
        $.ajax({url: "/switch?action=poweron",
                success: function (data) {
                    updateGui(data);
                }
            });
    } else {
        $.ajax({url: "/switch?action=poweroff",
                success: function (data) {
                    updateGui(data);
                }
            });
    }
}

function air(pv_status) {
    switch (pv_status) {
        case true:
            $.ajax({
                url: "/switch?action=airon",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case false:
            $.ajax({
                url: "/switch?action=airoff",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "shot":
            $.ajax({
                url: "/switch?action=shot",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "little":
            $.ajax({
                url: "/switch?action=little",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "big":
            $.ajax({
                url: "/switch?action=big",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "stop":
            $.ajax({
                url: "/switch?action=stop",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
    }
}

function updateTimer() {
    num = window.data.poweroff;
    if (num > 0) {
        $("#fill").show();
        $('.loading span').html(num + 's');
        window.data.poweroff = num - 1;
    } else {
        update();
    }
}

function updateGui(data) {
    response = data;
    if (response.heat == 1) {
        $("#heaton").hide();
        $("#heatoff").show();
    } else {
        $("#heaton").show();
        $("#heatoff").hide();
    }

    num = Math.ceil(response.poweroff / 1000);
    window.data = {};
    window.data.poweroff = num;
    if (updateInterval != null) {
        clearInterval(updateInterval);
    }

    if (response.air == 1 && response.poweroff > 0) {
        if (num > 0) {
            updateTimer();
            updateInterval = setInterval(updateTimer, 1000);
        }
        $("#shot").hide();
        $("#little").hide();
        $("#big").hide();
        $("#stop").show();
        $("#heatoff").hide();
        $("#heaton").hide();
    } else {
        $("#shot").show();
        $("#little").show();
        $("#big").show();
        $("#fill").hide();
        $("#stop").hide();
    }
}

function update() {
    $.ajax({
        url: "/status", success: function (result) {
            updateGui($.parseJSON(result));
        },
        error: function (result, error) {
            console.log(error);
        },
        dataType: 'text'
    });
}

function toggleLanguage() {
    // First, get the current status to know the current language
    $.getJSON('/status', function(data) {
        let currentLang = data.lang || 'de'; // Default to 'de' if not set
        let newLang = (currentLang === 'de') ? 'en' : 'de';

        // Call the backend to switch the language
        $.get('/switch?action=lang&lang=' + newLang, function() {
            // Reload the page to apply the new language
            location.reload();
        }).fail(function() {
            console.error("Error changing language.");
            // Optionally, inform the user that the language change failed
        });
    }).fail(function() {
        console.error("Error fetching current status.");
        // Optionally, inform the user that fetching status failed
    });
}

let settingsModal;
let currentLanguageDisplay;
let balloonTimeInput;

let modalCurrentLanguage = 'de'; // Variable to hold language state within the modal

function openSettingsModal() {
    if (!settingsModal || !currentLanguageDisplay || !balloonTimeInput) {
        console.error("Settings modal elements not yet initialized.");
        return;
    }
    // Fetch current status to populate modal
    $.getJSON('/status', function(data) {
        modalCurrentLanguage = data.lang || 'de';
        currentLanguageDisplay.textContent = modalCurrentLanguage === 'de' ? 'Deutsch' : 'Englisch';
        balloonTimeInput.value = data.maxSeconds || 60; // Default to 60s if not set
        settingsModal.style.display = 'block';
    }).fail(function() {
        console.error("Error fetching current status for settings modal.");
        // Fallback values if status fetch fails
        modalCurrentLanguage = 'de';
        currentLanguageDisplay.textContent = 'Deutsch';
        balloonTimeInput.value = 60;
        settingsModal.style.display = 'block';
    });
}

function closeSettingsModal() {
    if (!settingsModal) return;
    settingsModal.style.display = 'none';
}

function toggleLanguageModal() {
    if (!currentLanguageDisplay) return;
    modalCurrentLanguage = (modalCurrentLanguage === 'de') ? 'en' : 'de';
    currentLanguageDisplay.textContent = modalCurrentLanguage === 'de' ? 'Deutsch' : 'Englisch';
}

function saveSettings() {
    if (!balloonTimeInput || !settingsModal) return;
    let newBalloonTime = balloonTimeInput.value;

    $.get('/switch?action=lang&lang=' + modalCurrentLanguage, function() {
        console.log("Language setting saved: " + modalCurrentLanguage);
        $.get('/switch?action=calibration&cal=' + newBalloonTime, function() {
            console.log("Balloon time saved: " + newBalloonTime);
            closeSettingsModal();
            location.reload();
        }).fail(function() {
            console.error("Error saving balloon time.");
        });
    }).fail(function() {
        console.error("Error saving language setting.");
    });
}

$(document).ready(function() {
    // Initialize DOM element variables now that the DOM is ready
    settingsModal = document.getElementById('settingsModal');
    currentLanguageDisplay = document.getElementById('currentLanguageDisplay');
    balloonTimeInput = document.getElementById('balloonTimeInput');

    // Initialize and start the update mechanism (assuming 'update' function and 'updateInterval' are defined elsewhere)
    if (typeof update === 'function') {
        updateInterval = null;
        update();
        setInterval(update, 10000);
    } else {
        console.error("Global 'update' function not found. UI updates might not work.");
    }
});