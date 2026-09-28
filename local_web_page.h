#ifndef LOCAL_WEB_PAGE_H
#define LOCAL_WEB_PAGE_H

#include <Arduino.h>

static const char LOCAL_WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<!-- ==================== HEAD ==================== -->

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>HUB66S Device Configuration</title>
    <style>
        :root {
            --orange: #ff6b1a;
            --orange-dark: #df5008;
            --orange-soft: #fff0e6;
            --cream: #fff9f3;
            --card: #fff;
            --ink: #29251f;
            --muted: #81786f;
            --line: #eee3d8;
            --green: #16a56b;
            --red: #df4545;
            --shadow: 0 10px 30px #87380012
        }

        * {
            box-sizing: border-box
        }

        html,
        body {
            margin: 0;
            min-height: 100%;
            background: var(--cream);
            color: var(--ink);
            font: 14px Inter, Segoe UI, Arial, sans-serif
        }

        button,
        input,
        select {
            font: inherit
        }

        button {
            cursor: pointer
        }

        .hidden {
            display: none !important
        }

        .login-page {
            min-height: 100vh;
            display: grid;
            place-items: center;
            padding: 24px;
            background: radial-gradient(circle at 15% 15%, #ffd9bf 0, transparent 25%), linear-gradient(135deg, #fffaf5, #fff1e6)
        }

        .login-card {
            width: min(420px, 100%);
            padding: 38px;
            background: #fff;
            border: 1px solid #f4e4d6;
            border-radius: 28px;
            box-shadow: 0 24px 70px #8f3d1020
        }

        .brand {
            display: flex;
            align-items: center;
            gap: 11px;
            font-weight: 800;
            font-size: 22px
        }

        .brand-mark {
            width: 38px;
            height: 38px;
            display: grid;
            place-items: center;
            border-radius: 12px;
            background: var(--orange);
            color: #fff;
            font-size: 18px;
            box-shadow: 0 8px 20px #ff6b1a44
        }

        .login-card h1 {
            margin: 0px 0 8px;
            font-size: 26px
        }

        .login-brand {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 8px;
            margin-bottom: 26px;
        }

        .login-logo {
            width: 140px;
            max-width: 80%;
            height: auto;
            object-fit: contain;
        }

        .login-brand strong {
            font-size: 20px;
            font-weight: 800;
            letter-spacing: .14em;
        }

        .sub {
            margin: 0 0 25px;
            color: var(--muted)
        }

        .field {
            display: grid;
            gap: 7px;
            margin: 14px 0;
            font-weight: 650
        }

        .input-wrap {
            position: relative
        }

        .field input,
        .toolbar input,
        .toolbar select,
        .scan-field input {
            width: 100%;
            border: 1px solid var(--line);
            border-radius: 12px;
            background: #fff;
            padding: 12px 13px;
            outline: 0;
            color: var(--ink)
        }

        .field input:focus,
        .toolbar input:focus,
        .toolbar select:focus,
        .scan-field input:focus {
            border-color: var(--orange);
            box-shadow: 0 0 0 3px #ff6b1a18
        }

        .password-input {
            padding-right: 48px !important
        }

        .eye {
            position: absolute;
            right: 7px;
            top: 6px;
            width: 36px;
            height: 34px;
            border: 0;
            background: transparent;
            color: var(--muted);
            font-size: 18px
        }

        .remember {
            display: flex;
            align-items: center;
            gap: 8px;
            margin: 16px 0 22px;
            color: var(--muted)
        }

        .remember input {
            accent-color: var(--orange)
        }

        .btn {
            border: 1px solid var(--line);
            border-radius: 11px;
            background: #fff;
            color: var(--ink);
            padding: 10px 14px;
            font-weight: 700;
            white-space: nowrap
        }

        .btn:hover {
            border-color: #f0b58f
        }

        .btn.primary {
            background: var(--orange);
            border-color: var(--orange);
            color: #fff
        }

        .btn.primary:hover {
            background: var(--orange-dark)
        }

        .btn.danger {
            color: var(--red);
            background: #fff5f5;
            border-color: #ffd6d6
        }

        .btn.full {
            width: 100%;
            padding: 13px
        }

        .login-error {
            min-height: 18px;
            color: var(--red);
            font-size: 13px
        }

        .otp-field input {
            text-align: center;
            font-size: 22px;
            letter-spacing: .3em
        }

        .app {
            min-height: 100vh;
            display: grid;
            grid-template-columns: 230px 1fr
        }

        .sidebar {
            position: sticky;
            top: 0;
            height: 100vh;
            background: #fff;
            border-right: 1px solid var(--line);
            padding: 25px 18px;
            display: flex;
            flex-direction: column;
            z-index: 10
        }

        .sidebar .brand {
            padding: 2px 8px 30px;
            flex-direction: column;
            gap: 10px;
            text-align: center
        }

        .sidebar-logo {
            display: block;
            width: 124px;
            max-width: 100%;
            height: 58px;
            object-fit: contain
        }

        .sidebar-brand-name {
            color: var(--ink);
            font-size: 20px;
            line-height: 1;
            letter-spacing: .14em
        }

        .nav {
            display: grid;
            gap: 7px
        }

        .nav-btn {
            display: flex;
            align-items: center;
            gap: 12px;
            width: 100%;
            border: 0;
            border-radius: 12px;
            padding: 12px 14px;
            background: transparent;
            color: #6f675f;
            font-weight: 700;
            text-align: left;
            transition: background-color .18s ease, color .18s ease, transform .18s ease
        }

        .nav-btn:hover {
            background: #fff8f2;
            color: #d95a13;
            transform: translateX(2px)
        }

        .nav-btn.active {
            background: var(--orange-soft);
            color: var(--orange);
            box-shadow: inset 3px 0 0 var(--orange)
        }

        .nav-btn:focus-visible {
            outline: 3px solid #ff6b1a26;
            outline-offset: 2px
        }

        .nav-icon {
            width: 22px;
            height: 22px;
            display: grid;
            flex: 0 0 22px;
            place-items: center
        }

        .nav-icon svg {
            width: 20px;
            height: 20px;
            fill: none;
            stroke: currentColor;
            stroke-width: 1.8;
            stroke-linecap: round;
            stroke-linejoin: round
        }

        .sidebar-foot {
            margin-top: auto;
            display: grid;
            gap: 7px;
            padding-top: 14px;
            border-top: 1px solid #f4e9df
        }

        .main {
            min-width: 0;
            padding: 27px 30px 40px
        }

        .topbar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 15px;
            margin-bottom: 23px
        }

        .page-title h1 {
            margin: 0;
            font-size: 26px
        }

        .page-title p {
            margin: 6px 0 0;
            color: var(--muted)
        }

        .connection {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 9px 12px;
            background: #fff;
            border: 1px solid var(--line);
            border-radius: 999px;
            color: var(--muted);
            font-weight: 700
        }

        .dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--red)
        }

        .connection.ok .dot {
            background: var(--green)
        }

        .content {
            display: grid;
            gap: 18px
        }

        /* DASHBOARD OVERVIEW */

        .overview {
            display: grid;
            grid-template-columns: repeat(3, minmax(0, 1fr));
            gap: 18px
        }

        .stat {
            min-height: 125px;
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: flex-start;

            padding: 20px 24px;

            border-radius: 18px;
            border: 1px solid transparent;

            box-shadow: 0 8px 24px #0000000a;
        }

        /* Online */
        .stat.online {
            background: #eaf8f1;
            border-color: #ccebdd;
        }

        .stat.online strong,
        .stat.online .stat-title {
            color: #168a5b;
        }

        /* Offline */
        .stat.offline {
            background: #fff0f0;
            border-color: #f5d4d4;
        }

        .stat.offline strong,
        .stat.offline .stat-title {
            color: #d94a4a;
        }

        /* Groups */
        .stat.groups {
            background: #edf4ff;
            border-color: #d4e4fa;
        }

        .stat.groups strong,
        .stat.groups .stat-title {
            color: #3478c9;
        }

        /* Number */
        .stat strong {
            display: block;
            margin: 8px 0 5px;

            font-size: 32px;
            line-height: 1;
            font-weight: 800;
        }

        /* ONLINE / OFFLINE / GROUPS */
        .stat-title {
            font-size: 13px;
            font-weight: 800;
            letter-spacing: .06em;
            text-transform: uppercase;
        }

        /* ==================== GROUP ACTIONS ==================== */

        .btn.group-primary {
            background: #3478c9;
            border-color: #3478c9;
            color: #fff;
        }

        .btn.group-primary:hover {
            background: #2868b5;
            border-color: #2868b5;
        }

        .btn.group-secondary {
            background: #edf4ff;
            border-color: #d4e4fa;
            color: #3478c9;
        }

        .btn.group-secondary:hover {
            background: #e1edff;
            border-color: #bcd5f5;
        }

        /* Description */
        .stat small {
            color: var(--muted);
            font-size: 12px;
            font-weight: 600;
        }

        .card {
            background: var(--card);
            border: 1px solid var(--line);
            border-radius: 20px;
            padding: 20px;
            box-shadow: var(--shadow)
        }

        .card-head {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;
            margin-bottom: 16px
        }

        .card-head h2 {
            font-size: 17px;
            margin: 0
        }

        .card-head p {
            margin: 5px 0 0;
            color: var(--muted)
        }

        .device-availability {
            display: flex;
            align-items: baseline;
            gap: 8px;
            flex-wrap: wrap
        }

        .device-availability strong {
            color: var(--orange);
            font-size: 28px;
            line-height: 1;
            font-weight: 850
        }

        .device-availability span {
            color: var(--muted);
            font-weight: 700
        }

        .discovery-grid {
            display: grid;
            grid-template-columns: minmax(210px, 280px) auto 1fr;
            gap: 12px;
            align-items: end
        }

        .scan-field {
            display: grid;
            gap: 7px;
            font-weight: 700
        }

        .scan-input {
            display: grid;
            grid-template-columns: 1fr auto;
            align-items: center;
            border: 1px solid var(--line);
            border-radius: 12px;
            background: #fff;
            overflow: hidden
        }

        .scan-input input {
            border: 0 !important;
            border-radius: 0 !important;
            box-shadow: none !important
        }

        .scan-input span {
            padding: 0 13px;
            color: var(--muted);
            font-weight: 650
        }

        .actions {
            display: flex;
            gap: 8px;
            flex-wrap: wrap
        }

        .job {
            min-height: 44px;
            display: flex;
            align-items: center;
            padding: 10px 14px;
            border-radius: 12px;
            background: #fff8f2;
            color: var(--muted);
            overflow-wrap: anywhere
        }

        .progress {
            height: 7px;
            margin-top: 14px;
            border-radius: 99px;
            background: #f3e9e0;
            overflow: hidden
        }

        .progress-bar {
            height: 100%;
            width: 0;
            background: var(--orange);
            transition: width .25s
        }

        .toolbar {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(min(100%, 30rem), 1fr));
            align-items: start;
            gap: 12px;
            margin-bottom: 15px
        }

        .toolbar-filters {
            grid-column: 1/-1;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(min(100%, 14rem), 1fr));
            gap: 9px
        }

        #groupFilter,
        #filterDevice,
        #filterLid {
            min-width: 0;
            width: 100%
        }

        .group-actions,
        .toolbar-actions {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(min(100%, 7.5rem), 1fr));
            gap: 7px;
            align-items: stretch
        }

        .group-actions .btn,
        .toolbar-actions .btn {
            width: 100%;
            white-space: nowrap
        }

        .table-wrap {
            overflow: auto;
            border: 1px solid var(--line);
            border-radius: 14px
        }

        .pagination-bar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 16px;
            margin-top: 15px;
            color: var(--muted)
        }

        .pagination-controls,
        .pagination-pages {
            display: flex;
            align-items: center;
            gap: 5px
        }

        .page-btn {
            min-width: 36px;
            height: 36px;
            border: 1px solid transparent;
            border-radius: 9px;
            background: transparent;
            color: var(--ink);
            font-weight: 750
        }

        .page-btn:hover:not(:disabled) {
            border-color: #f0b58f;
            background: #fff8f2
        }

        .page-btn.active {
            border-color: var(--orange);
            background: var(--orange);
            color: #fff
        }

        .page-btn:disabled {
            cursor: default;
            color: #bbb2aa
        }

        .page-arrow {
            padding: 0 10px;
            white-space: nowrap
        }

        table {
            width: 100%;
            border-collapse: collapse;
            min-width: 850px
        }

        th,
        td {
            padding: 12px 13px;
            text-align: left;
            border-bottom: 1px solid #f3ebe4;
            white-space: nowrap
        }

        th {
            background: #fffaf6;
            color: #776e65;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: .04em
        }

        tr:last-child td {
            border-bottom: 0
        }

        tbody tr:hover {
            background: #fffaf6
        }

        .pick {
            accent-color: var(--orange)
        }

        .device-name {
            display: inline-flex;
            align-items: center;
            gap: 7px;
            font-weight: 700
        }

        .edit-alias {
            border: 0;
            background: transparent;
            color: var(--orange);
            padding: 2px;
            font-size: 16px
        }

        .status-pill {
            display: inline-flex;
            align-items: center;
            border-radius: 999px;
            padding: 4px 9px;
            font-size: 11px;
            font-weight: 800
        }

        .status-pill.ONLINE {
            color: var(--green);
            background: #e8f8f1
        }

        .status-pill.NO_RESPONSE,
        .status-pill.OFFLINE {
            color: var(--red);
            background: #ffeded
        }

        .status-pill.REFRESHING {
            color: var(--orange);
            background: var(--orange-soft)
        }

        .status-pill.UNKNOWN {
            color: var(--muted);
            background: #f2efec
        }

        .section-actions {
            display: flex;
            gap: 8px;
            flex-wrap: wrap
        }

        .log {
            white-space: pre-wrap;
            font: 12px Consolas, monospace;
            background: #2e2925;
            color: #f6e9de;
            padding: 14px;
            border-radius: 13px;
            max-height: 180px;
            overflow: auto
        }

        .firmware-content {
            max-width: 760px
        }

        .firmware-card {
            display: grid;
            gap: 22px
        }

        .version-block {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 18px;
            padding: 17px 18px;
            border: 1px solid var(--line);
            border-radius: 14px;
            background: #fffaf6
        }

        .version-block span {
            color: var(--muted);
            font-weight: 700
        }

        .version-block strong {
            color: var(--orange);
            font-size: 20px
        }

        .firmware-file {
            margin: 0
        }

        .firmware-file input {
            padding: 9px
        }

        .firmware-file input::file-selector-button {
            margin-right: 12px;
            border: 1px solid var(--line);
            border-radius: 9px;
            background: #fff;
            color: var(--ink);
            padding: 8px 11px;
            font-weight: 700;
            cursor: pointer
        }

        .ota-progress-row {
            display: flex;
            align-items: center;
            gap: 12px
        }

        .ota-progress-row .progress {
            flex: 1;
            margin: 0;
            height: 10px
        }

        .ota-progress-row strong {
            min-width: 42px;
            color: var(--orange);
            text-align: right
        }

        .ota-warning {
            margin: 0;
            padding: 13px 15px;
            border: 1px solid #ffd8be;
            border-radius: 12px;
            background: var(--orange-soft);
            color: #a9430a;
            font-weight: 700
        }

        .modal {
            position: fixed;
            inset: 0;
            background: #2d211a80;
            display: none;
            place-items: center;
            padding: 20px;
            z-index: 50
        }

        .modal.open {
            display: grid
        }

        .dialog {
            width: min(520px, 100%);
            max-height: 90vh;
            overflow: auto;
            background: #fff;
            border-radius: 22px;
            padding: 23px;
            box-shadow: 0 30px 80px #1d0b0250
        }

        .dialog h3 {
            margin: 0 0 18px
        }

        .form {
            display: grid;
            gap: 12px
        }

        .member-list {
            border: 1px solid var(--line);
            border-radius: 13px;
            max-height: 290px;
            overflow: auto;
            padding: 7px
        }

        .member {
            display: flex;
            gap: 10px;
            align-items: center;
            padding: 10px;
            border-bottom: 1px solid var(--line)
        }

        .member:last-child {
            border-bottom: 0
        }

        .member input {
            accent-color: var(--orange)
        }

        .dialog-actions {
            display: flex;
            gap: 8px;
            justify-content: flex-end;
            flex-wrap: wrap;
            margin-top: 6px
        }

        .muted {
            color: var(--muted)
        }

        .empty {
            padding: 28px !important;
            text-align: center;
            color: var(--muted)
        }

        .account-actions {
            display: flex;
            gap: 7px;
            flex-wrap: wrap
        }

        .qr-code {
            display: grid;
            width: min(260px, 100%);
            aspect-ratio: 1;
            margin: 12px auto;
            padding: 12px;
            background: #fff;
            border: 1px solid var(--line)
        }

        .qr-code i {
            display: block
        }

        .setup-key {
            overflow-wrap: anywhere;
            padding: 10px;
            border-radius: 10px;
            background: var(--cream);
            font: 13px Consolas, monospace
        }

        @media(max-width:1100px) {
            .discovery-grid {
                grid-template-columns: 1fr auto
            }

            .job {
                grid-column: 1/-1
            }
        }

        @media(max-width:820px) {
            .app {
                grid-template-columns: 76px 1fr
            }

            .sidebar {
                padding: 20px 10px
            }

            .sidebar .brand strong,
            .nav-label {
                display: none
            }

            .sidebar .brand {
                justify-content: center;
                padding-left: 0;
                padding-right: 0
            }

            .sidebar-logo {
                width: 48px;
                height: 38px
            }

            .nav-btn {
                justify-content: center;
                padding: 12px
            }

            .main {
                padding: 22px 18px
            }

            .topbar {
                align-items: flex-start
            }

            .connection {
                font-size: 12px
            }
        }

        @media(max-width:570px) {
            .app {
                display: block
            }

            .sidebar {
                position: fixed;
                left: 0;
                right: 0;
                bottom: 0;
                top: auto;
                width: auto;
                height: 68px;
                display: block;
                padding: 8px;
                border: 0;
                border-top: 1px solid var(--line)
            }

            .sidebar .brand {
                display: none
            }

            .nav {
                grid-template-columns: repeat(4, 1fr)
            }

            .nav-btn {
                padding: 8px
            }

            .nav-icon {
                font-size: 16px
            }

            .nav-label {
                display: block;
                font-size: 10px
            }

            .sidebar-foot {
                position: fixed;
                right: 10px;
                top: 10px;
                display: flex;
                gap: 6px;
                padding: 0;
                border: 0
            }

            .sidebar-foot .settings,
            .sidebar-foot .logout {
                justify-content: center;
                padding: 9px;
                width: 42px;
                background: #fff;
                border: 1px solid var(--line)
            }

            .sidebar-foot .settings .nav-label,
            .sidebar-foot .logout .nav-label {
                display: none
            }

            .sidebar-foot .logout {
                position: static
            }

            .main {
                padding: 70px 12px 90px
            }

            .discovery-grid {
                grid-template-columns: 1fr
            }

            .job {
                grid-column: auto
            }

            .pagination-bar {
                align-items: flex-start;
                flex-direction: column
            }

            .page-title h1 {
                font-size: 22px
            }

            .connection {
                position: absolute;
                top: 16px;
                left: 12px
            }

            .card {
                padding: 15px
            }
        }
    </style>
</head>

<!-- ==================== LOGIN PAGE ==================== -->

<body>
    <section id="loginView" class="login-page">
        <form id="loginForm" class="login-card">

            <div class="login-brand">
                <img class="login-logo" src="/logo.png" alt="ALTA Medi@">

                <strong>HUB66S</strong>
            </div>

            <h1>Device Control & Management</h1>

            <p class="sub">
                Sign in to access device configuration and management.
            </p>

            <label class="field">
                Username
                <input id="loginUsername" autocomplete="username" required>
            </label>

            <label id="loginPasswordField" class="field">
                Password
                <div class="input-wrap">
                    <input id="loginPassword" class="password-input" type="password" autocomplete="current-password"
                        required>

                    <button id="togglePassword" class="eye" type="button" title="Show password">
                        &#128065;
                    </button>
                </div>
            </label>

            <label id="loginOtpField" class="field otp-field hidden">
                Google Authenticator Code
                <input id="loginOtp" inputmode="numeric" autocomplete="one-time-code" maxlength="6">
            </label>

            <label class="remember">
                <input id="rememberLogin" type="checkbox">
                Remember me
            </label>

            <div id="loginError" class="login-error"></div>

            <button class="btn primary full" type="submit">
                Sign In
            </button>

        </form>
    </section>

    <!-- ==================== DASHBOARD ==================== -->
    <div id="dashboardView" class="app hidden">
        <aside class="sidebar">
            <div class="brand"><img class="sidebar-logo" src="/logo.png" alt="ALTA Medi@ logo"><strong
                    class="sidebar-brand-name">HUB66S</strong></div>
            <nav class="nav"><button class="nav-btn active" data-section="overview"><span class="nav-icon"><svg
                            viewBox="0 0 24 24" aria-hidden="true">
                            <path d="M3 11.5 12 4l9 7.5"></path>
                            <path d="M5.5 10v10h13V10M9.5 20v-6h5v6"></path>
                        </svg></span><span class="nav-label">Dashboard</span></button><button class="nav-btn"
                    data-section="devices"><span class="nav-icon"><svg viewBox="0 0 24 24" aria-hidden="true">
                            <rect x="5" y="3" width="14" height="18" rx="2"></rect>
                            <path d="M9 7h6M9 11h6M9 15h3"></path>
                            <circle cx="16" cy="16" r="1"></circle>
                        </svg></span><span class="nav-label">Devices</span></button><button class="nav-btn"
                    data-section="groups"><span class="nav-icon"><svg viewBox="0 0 24 24" aria-hidden="true">
                            <circle cx="9" cy="8" r="3"></circle>
                            <circle cx="17" cy="9" r="2.5"></circle>
                            <path d="M3.5 19c.4-3.4 2.2-5 5.5-5s5.1 1.6 5.5 5M14.5 14.5c3.4-.8 5.6.7 6 3.5"></path>
                        </svg></span><span class="nav-label">Groups</span></button><button class="nav-btn"
                    type="button" data-section="accounts"><span class="nav-icon"><svg viewBox="0 0 24 24" aria-hidden="true">
                            <circle cx="12" cy="8" r="3.5"></circle>
                            <path d="M5 20c.5-4.2 2.8-6.5 7-6.5s6.5 2.3 7 6.5"></path>
                        </svg></span><span class="nav-label">Accounts</span></button>
            </nav>
            <div class="sidebar-foot"><button class="nav-btn settings" data-section="settings"><span
                        class="nav-icon"><svg viewBox="0 0 24 24" aria-hidden="true">
                            <circle cx="12" cy="12" r="3"></circle>
                            <path
                                d="M19.4 15a1.7 1.7 0 0 0 .3 1.9l.1.1-2.8 2.8-.1-.1a1.7 1.7 0 0 0-1.9-.3 1.7 1.7 0 0 0-1 1.6v.2h-4V21a1.7 1.7 0 0 0-1-1.6 1.7 1.7 0 0 0-1.9.3l-.1.1L4.2 17l.1-.1a1.7 1.7 0 0 0 .3-1.9A1.7 1.7 0 0 0 3 14H2.8v-4H3a1.7 1.7 0 0 0 1.6-1 1.7 1.7 0 0 0-.3-1.9L4.2 7 7 4.2l.1.1A1.7 1.7 0 0 0 9 4.6 1.7 1.7 0 0 0 10 3v-.2h4V3a1.7 1.7 0 0 0 1 1.6 1.7 1.7 0 0 0 1.9-.3l.1-.1L19.8 7l-.1.1a1.7 1.7 0 0 0-.3 1.9 1.7 1.7 0 0 0 1.6 1h.2v4H21a1.7 1.7 0 0 0-1.6 1Z">
                            </path>
                        </svg></span><span class="nav-label">Settings</span></button><button id="logoutButton"
                    class="nav-btn logout"><span class="nav-icon"><svg viewBox="0 0 24 24" aria-hidden="true">
                            <path d="M10 4H5v16h5M14 8l4 4-4 4M8 12h10"></path>
                        </svg></span><span class="nav-label">Sign Out</span></button></div>
        </aside>
        <!-- ===== MAIN CONTENT ===== -->
        <main class="main">
            <header class="topbar">
                <div class="page-title">
                    <h1>CONTROLS SYSTEM</h1>
                    <p id="hub">Local access point</p>
                </div>
                <div id="connection" class="connection"><span class="dot"></span><span
                        id="connectionText">Connecting...</span></div>
            </header>
            <div id="controlsView" class="content">

                <!-- Dashboard Overview -->
                <section id="overviewSection" class="overview">

                    <!-- Online Devices -->
                    <article class="stat online">
                        <span class="stat-title">Online</span>
                        <strong id="onlineCount">0</strong>
                        <small>Devices available</small>
                    </article>

                    <!-- Offline Devices -->
                    <article class="stat offline">
                        <span class="stat-title">Offline</span>
                        <strong id="offlineCount">0</strong>
                        <small>Devices unavailable</small>
                    </article>

                    <!-- Device Groups -->
                    <article class="stat groups">
                        <span class="stat-title">Groups</span>
                        <strong id="groupCount">0</strong>
                        <small>Configured groups</small>
                    </article>

                </section>

                <!-- Device Detection -->
                <section id="discoverySection" class="card">
                    <div class="card-head">
                        <div>
                            <h2>Device Detection</h2>
                            <p>Scan and identify available HUB66S devices.</p>
                        </div>
                    </div>
                    <div class="discovery-grid">
                        <label class="scan-field">
                            Device Count
                            <div class="scan-input">
                                <input id="scanLimit" type="number" inputmode="numeric" min="1" max="100" step="1"
                                    value="15"><span>Devices</span>
                            </div>
                        </label>
                        <div class="actions"><button class="btn primary" onclick="startScan()">Scan</button><button
                                class="btn" onclick="send('scan.refresh')">Refresh</button><button class="btn danger"
                                onclick="send('devices.clear')">Clear</button></div>
                        <div id="job" class="job">Ready - enter a Device Count, then click Scan.</div>
                    </div>
                    <div class="progress">
                        <div id="scanProgress" class="progress-bar"></div>
                    </div>
                </section>

                <!-- Device List -->
                <section id="deviceSection" class="card">
                    <div class="card-head">
                        <div>
                            <h2 id="listTitle">All Devices</h2>
                            <p class="device-availability"><strong id="deviceCount">0 / 0</strong><span>Devices
                                    Available</span></p>
                        </div>
                    </div>
                    <div class="toolbar">
                        <div class="toolbar-filters"><select id="groupFilter" onchange="resetDevicePage()">
                                <option value="all">All Devices</option>
                            </select><input id="filterDevice" placeholder="Search Device UID"
                                oninput="resetDevicePage()"><input id="filterLid" type="number" placeholder="Search LID"
                                oninput="resetDevicePage()"></div>
                        <div id="groupSection" class="group-actions">
                        <button class="btn group-primary" onclick="openGroup()">Add
                                Group</button><button class="btn group-secondary" onclick="editGroup()">Edit Group</button><button
                                class="btn danger" onclick="confirmDeleteGroup()">Delete Group</button></div>
                        <div class="toolbar-actions"><button class="btn primary" onclick="openLicense()">Set
                                License</button><button class="btn" onclick="getSelectedLicense()">Get
                                License</button><button class="btn" onclick="openConfigDevice()">Config
                                Device</button><button class="btn danger" onclick="confirmDeleteNode()">Remove</button>
                        </div>
                    </div>
                    <div class="table-wrap">
                        <table>
                            <thead>
                                <tr>
                                    <th><input id="checkAll" class="pick" type="checkbox"
                                            title="Select all visible Devices" aria-label="Select all visible Devices"
                                            onchange="toggleAllDevices(this.checked)"></th>
                                    <th>No.</th>
                                    <th>Device ID</th>
                                    <th>MAC</th>
                                    <th>LID</th>
                                    <th>TIME (Minutes)</th>
                                    <th>Status</th>
                                    <th>Last Seen</th>
                                </tr>
                            </thead>
                            <tbody id="devicesEl"></tbody>
                        </table>
                    </div>
                    <div class="pagination-bar">
                        <span id="paginationSummary">Showing 0–0 of 0 devices</span>
                        <div class="pagination-controls"><button id="previousPage" class="page-btn page-arrow"
                                type="button" onclick="changeDevicePage(-1)">&#8249; Previous</button>
                            <div id="paginationPages" class="pagination-pages"></div><button id="nextPage"
                                class="page-btn page-arrow" type="button" onclick="changeDevicePage(1)">Next
                                &#8250;</button>
                        </div>
                    </div>
                </section>
                <section class="card">
                    <div class="card-head">
                        <div>
                            <h2>Activity Log</h2>
                            <p>WebSocket operations and device activity.</p>
                        </div>
                    </div>
                    <div id="logEl" class="log">Dashboard ready.</div>
                </section>
            </div>
            <div id="firmwareView" class="content firmware-content hidden">
                <section id="settingsSection" class="card firmware-card">
                    <div class="card-head">
                        <div>
                            <h2>Firmware Update</h2>
                        </div>
                    </div>
                    <div class="version-block"><span>Current Version</span><strong id="currentVersion">--</strong>
                    </div>
                    <label class="field firmware-file">Firmware file (.bin)<input id="firmwareFile" type="file"
                            accept=".bin,application/octet-stream"></label>
                    <div><button class="btn primary" type="button" onclick="previewFirmwareUpdate()">Upload &amp;
                            Update</button></div>
                    <div class="ota-progress-row">
                        <div class="progress">
                            <div id="otaProgress" class="progress-bar"></div>
                        </div><strong id="otaPercent">0%</strong>
                    </div>
                    <p class="ota-warning">Do not turn off the power while the firmware is updating.</p>
                </section>
            </div>
            <div id="accountsView" class="content hidden">
                <section class="card">
                    <div class="card-head">
                        <div>
                            <h2>Accounts</h2>
                            <p>Create usernames, change passwords and configure Google Authenticator.</p>
                        </div>
                        <button class="btn primary" type="button" onclick="openAccount()">Add Account</button>
                    </div>
                    <div class="table-wrap">
                        <table>
                            <thead><tr><th>Username</th><th>2FA</th><th>Actions</th></tr></thead>
                            <tbody id="accountsEl"></tbody>
                        </table>
                    </div>
                </section>
            </div>
        </main>
    </div>
    <div id="licenseModal" class="modal">
        <div class="dialog">
            <h3>Set License</h3>
            <div class="form"><label class="field">Target<input id="licDeviceName" readonly></label><label
                    id="licLidField" class="field">License ID<input id="licLid" type="number" min="1"></label><label
                    class="field">Hours<input id="licHours" type="number" min="0"></label><label
                    class="field">Minutes<input id="licMinutes" type="number" min="0" max="59"></label>
                <div class="dialog-actions"><button class="btn"
                        onclick="closeModal(licenseModal)">Cancel</button><button class="btn primary"
                        onclick="setLicense()">Send</button></div>
            </div>
        </div>
    </div>
    <div id="configModal" class="modal">
        <div class="dialog">
            <h3>Config Device</h3>
            <div class="form"><label class="field">Device UID<input id="configDeviceUid" readonly></label><label
                    class="field">Current LID<input id="configCurrentLid" readonly></label><label class="field">New
                    LID<input id="configNewLid" type="text" maxlength="11"></label>
                <div class="dialog-actions"><button class="btn" onclick="closeModal(configModal)">Cancel</button><button
                        class="btn primary" onclick="saveConfigDevice()">Save</button></div>
            </div>
        </div>
    </div>
    <div id="groupModal" class="modal">
        <div class="dialog">
            <h3 id="groupModalTitle">Add Group</h3>
            <div class="form"><label class="field">Group Name<input id="groupName" maxlength="39"></label><label
                    class="member"><input id="groupCheckAll" type="checkbox"
                        onchange="toggleAllGroupMembers(this.checked)"><strong>Select All Devices</strong></label>
                <div id="groupMembers" class="member-list"></div>
                <div class="muted">Selected: <strong id="selectedMemberCount">0</strong> Devices</div>
                <div class="dialog-actions"><button class="btn" onclick="closeModal(groupModal)">Cancel</button><button
                        id="saveGroupButton" class="btn primary" onclick="saveGroup()">Create Group</button></div>
            </div>
        </div>
    </div>
    <div id="aliasModal" class="modal">
        <div class="dialog">
            <h3>Device Alias</h3>
            <div class="form"><label class="field">Actual Device ID<input id="aliasDeviceId" readonly></label><label
                    class="field">Alias<input id="aliasName" maxlength="40" placeholder="Enter a device alias"></label>
                <div class="dialog-actions"><button id="deleteAliasButton" class="btn danger"
                        onclick="deleteAlias()">Delete Alias</button><button class="btn"
                        onclick="closeModal(aliasModal)">Cancel</button><button class="btn primary"
                        onclick="saveAlias()">Save</button></div>
            </div>
        </div>
    </div>
    <div id="deleteNodeModal" class="modal">
        <div class="dialog">
            <h3>Remove from All Devices?</h3>
            <p class="muted">This only removes the Device from the temporary RAM list. Devices saved in Groups are not
                affected.</p>
            <div class="dialog-actions"><button class="btn" onclick="closeModal(deleteNodeModal)">Cancel</button><button
                    class="btn danger" onclick="deleteNode()">Remove</button></div>
        </div>
    </div>
    <div id="deleteGroupModal" class="modal">
        <div class="dialog">
            <h3>Delete Group?</h3>
            <p id="deleteGroupText" class="muted"></p>
            <div class="dialog-actions"><button class="btn"
                    onclick="closeModal(deleteGroupModal)">Cancel</button><button class="btn danger"
                    onclick="deleteGroup()">Delete Group</button></div>
        </div>
    </div>
    <div id="accountModal" class="modal">
        <div class="dialog">
            <h3 id="accountModalTitle">Add Account</h3>
            <div class="form">
                <label class="field">Username<input id="accountUsername" maxlength="32"></label>
                <label class="field">Password<input id="accountPassword" type="password" minlength="6"></label>
                <div class="dialog-actions"><button class="btn" onclick="closeModal(accountModal)">Cancel</button><button
                        class="btn primary" onclick="saveAccount()">Save</button></div>
            </div>
        </div>
    </div>
    <div id="twoFactorModal" class="modal">
        <div class="dialog">
            <h3>Enable Google Authenticator</h3>
            <p class="muted">Scan this QR code, then enter the 6-digit code to confirm.</p>
            <div id="twoFactorQr" class="qr-code"></div>
            <p class="muted">Setup key:</p>
            <div id="twoFactorSecret" class="setup-key"></div>
            <label class="field otp-field">Authentication Code<input id="twoFactorCode" inputmode="numeric"
                    maxlength="6"></label>
            <div class="dialog-actions"><button class="btn" onclick="closeModal(twoFactorModal)">Cancel</button><button
                    class="btn primary" onclick="confirmTwoFactor()">Enable 2FA</button></div>
        </div>
    </div>
    <script>
        const DEVICE_PAGE_SIZE = 15; let ws, devices = [], groups = [], accounts = [], currentUser = '', editingAccount = '', waitingOtp = false, editingGroupId = 0, editingAliasMac = '', devicePage = 1; const $ = id => document.getElementById(id), esc = s => String(s ?? '').replace(/[&<>"']/g, c => c === '&' ? '&amp;' : c === '<' ? '&lt;' : c === '>' ? '&gt;' : c === '"' ? '&quot;' : '&#39;'), deviceUid = d => d.id_src || d.uid, displayDeviceId = d => d.alias || deviceUid(d), selectedNodes = () => [...document.querySelectorAll('#devicesEl .pick:checked')].map(x => x.value), oneNode = () => { const s = selectedNodes(); return s.length === 1 ? s[0] : null }, groupDevices = g => g?.devices || [], currentGroup = () => groups.find(g => String(g.id) === groupFilter.value) || null;
        function showDashboard() { loginView.classList.add('hidden'); dashboardView.classList.remove('hidden') } function showLogin() { dashboardView.classList.add('hidden'); loginView.classList.remove('hidden'); loginOtpField.classList.add('hidden'); loginPasswordField.classList.remove('hidden'); loginPassword.required = true; waitingOtp = false; loginOtp.value = ''; loginPassword.value = '' } function logout() { send('auth.logout'); showLogin() } function initLogin() { loginUsername.value = localStorage.getItem('hub66User') || 'admin'; rememberLogin.checked = !!localStorage.getItem('hub66User'); loginForm.addEventListener('submit', e => { e.preventDefault(); loginError.textContent = ''; if (waitingOtp) { if (!/^\d{6}$/.test(loginOtp.value)) return loginError.textContent = 'Enter the 6-digit authentication code.'; send('auth.verify_otp', { code: loginOtp.value, epoch: Math.floor(Date.now() / 1000) }); return } if (!loginUsername.value.trim() || !loginPassword.value) return loginError.textContent = 'Enter your username and password.'; send('auth.login', { username: loginUsername.value.trim(), password: loginPassword.value, epoch: Math.floor(Date.now() / 1000) }) }); togglePassword.onclick = () => { const show = loginPassword.type === 'password'; loginPassword.type = show ? 'text' : 'password'; togglePassword.title = show ? 'Hide password' : 'Show password' }; logoutButton.onclick = logout; connect() }
        function connect() { if (ws && (ws.readyState === 0 || ws.readyState === 1)) return; ws = new WebSocket(`ws://${location.hostname}:81/`); ws.onopen = () => { connection.classList.add('ok'); connectionText.textContent = 'Connected' }; ws.onclose = () => { connection.classList.remove('ok'); connectionText.textContent = 'Disconnected'; setTimeout(connect, 1500) }; ws.onmessage = e => { try { handle(JSON.parse(e.data)) } catch (_) { log(e.data) } } } function send(action, data = {}) { if (!ws || ws.readyState !== 1) { loginError.textContent = 'WebSocket is not connected'; return } ws.send(JSON.stringify({ action, data })) } function requestState() { send('state.get') }
        function startScan() { const limit = Number(scanLimit.value); if (!Number.isInteger(limit) || limit < 1 || limit > 100) return alert('Enter a Scan Limit from 1 to 100'); scanProgress.style.width = '0%'; send('scan.start', { limit }) }
        function handle(m) { if (m.type === 'auth.required') showLogin(); else if (m.type === 'auth.otp_required') { waitingOtp = true; loginPassword.value = ''; loginPassword.required = false; loginPasswordField.classList.add('hidden'); loginOtpField.classList.remove('hidden'); loginOtp.focus() } else if (m.type === 'auth.success') { currentUser = m.username; waitingOtp = false; if (rememberLogin.checked) localStorage.setItem('hub66User', loginUsername.value.trim()); else localStorage.removeItem('hub66User'); showDashboard() } else if (m.type === 'accounts') { accounts = m.accounts || []; renderAccounts() } else if (m.type === 'account.2fa.setup') showTwoFactorSetup(m); else if (m.type === 'state') { devices = m.devices || []; groups = m.groups || []; hub.textContent = `AP ${m.ip} | ${m.hubMac}`; render() } else if (m.type === 'device.upsert') { const i = devices.findIndex(d => d.mac === m.data.mac); i < 0 ? devices.push(m.data) : devices[i] = { ...devices[i], ...m.data }; render() } else if (m.type === 'device.deleted') { devices = devices.filter(d => d.mac !== m.mac); render() } else if (m.type === 'devices.cleared') { devices = []; groupFilter.value = 'all'; scanProgress.style.width = '0%'; render() } else if (m.type === 'groups.changed') { const old = groupFilter.value; groups = m.groups || []; renderGroupFilter(old); render() } else if (m.type === 'job') { const done = Number(m.done) || 0, total = Number(m.total) || 0; scanProgress.style.width = total ? `${Math.min(100, done * 100 / total)}%` : '0%'; job.textContent = `${m.state}: ${done}/${total} - Unique ${m.success || 0} - No response ${m.noResponse || 0} - ${m.message || ''}`; log(job.textContent) } else if (m.type === 'error') { const message = m.message || m.code; if (dashboardView.classList.contains('hidden')) loginError.textContent = message; else alert(message) } else if (m.type === 'ack') log(`Acknowledged: ${m.action}`) }
        function overviewScope() { const seen = new Map(devices.map(d => [d.mac, d])); groups.flatMap(groupDevices).forEach(d => { if (!seen.has(d.mac)) seen.set(d.mac, d) }); return [...seen.values()] } function renderOverview() { const all = overviewScope(); onlineCount.textContent = all.filter(d => d.responseState === 'ONLINE').length; offlineCount.textContent = all.filter(d => d.responseState !== 'ONLINE').length; groupCount.textContent = groups.length } function render() { renderGroupFilter(groupFilter.value); renderNodes(); renderOverview() } function renderGroupFilter(value = 'all') { groupFilter.innerHTML = '<option value="all">All Devices</option>' + groups.map(g => `<option value="${g.id}">${esc(g.name)}</option>`).join(''); groupFilter.value = groups.some(g => String(g.id) === String(value)) ? String(value) : 'all' } function groupNumbers(group) { const scope = group ? groupDevices(group) : devices, online = scope.filter(d => d.responseState === 'ONLINE').length, total = group ? (group.memberCount ?? (group.members || []).length) : devices.length; return `${online}/${total}` }
        function resetDevicePage() { devicePage = 1; renderNodes() } function changeDevicePage(delta) { devicePage += delta; renderNodes() } function goToDevicePage(page) { devicePage = page; renderNodes() }
        function renderNodes() { const id = filterDevice.value.trim().toLowerCase(), lid = filterLid.value.trim(), group = currentGroup(), scope = group ? groupDevices(group) : devices, rows = scope.filter(d => (!id || String(deviceUid(d)).toLowerCase().includes(id) || String(d.alias || '').toLowerCase().includes(id)) && (!lid || String(d.lid) === lid)), totalPages = Math.max(1, Math.ceil(rows.length / DEVICE_PAGE_SIZE)); devicePage = Math.min(Math.max(1, devicePage), totalPages); const start = (devicePage - 1) * DEVICE_PAGE_SIZE, pageRows = rows.slice(start, start + DEVICE_PAGE_SIZE), end = start + pageRows.length; listTitle.textContent = group ? `Group: ${group.name}` : 'All Devices'; deviceCount.textContent = groupNumbers(group).replace('/', ' / '); devicesEl.innerHTML = pageRows.length ? pageRows.map((d, i) => `<tr><td><input class="pick" type="checkbox" value="${esc(d.mac)}"></td><td>${start + i + 1}</td><td><span class="device-name" title="Actual Device ID: ${esc(deviceUid(d))}">${esc(displayDeviceId(d))}<button class="edit-alias" type="button" title="Set or edit alias" data-mac="${esc(d.mac)}">&#9998;</button></span></td><td>${esc(d.mac)}</td><td>${esc(d.lid)}</td><td>${esc(d.remain)}</td><td><span class="status-pill ${esc(d.responseState)}">${esc(d.responseState)}</span></td><td>${Math.floor((d.lastSeenAgoMs || 0) / 1000)}s</td></tr>`).join('') : '<tr><td class="empty" colspan="8">No Devices in this view.</td></tr>'; paginationSummary.textContent = rows.length ? `Showing ${start + 1}\u2013${end} of ${rows.length} devices` : 'Showing 0\u20130 of 0 devices'; paginationPages.innerHTML = Array.from({ length: totalPages }, (_, i) => `<button class="page-btn ${devicePage === i + 1 ? 'active' : ''}" type="button" onclick="goToDevicePage(${i + 1})">${i + 1}</button>`).join(''); previousPage.disabled = devicePage === 1; nextPage.disabled = devicePage === totalPages; checkAll.checked = false; checkAll.indeterminate = false; checkAll.disabled = !pageRows.length }
        function toggleAllDevices(checked) { document.querySelectorAll('#devicesEl .pick').forEach(box => box.checked = checked); syncCheckAll() } function syncCheckAll() { const boxes = [...document.querySelectorAll('#devicesEl .pick')], selected = boxes.filter(box => box.checked).length; checkAll.disabled = !boxes.length; checkAll.checked = boxes.length > 0 && selected === boxes.length; checkAll.indeterminate = selected > 0 && selected < boxes.length }
        function openAlias(mac) { const d = devices.find(x => x.mac === mac) || groups.flatMap(groupDevices).find(x => x.mac === mac); if (!d) return; editingAliasMac = mac; aliasDeviceId.value = deviceUid(d); aliasName.value = d.alias || ''; deleteAliasButton.disabled = !d.alias; aliasModal.classList.add('open'); aliasName.focus(); aliasName.select() } function saveAlias() { const alias = aliasName.value.trim(); if (!alias) return alert('Enter a device alias'); send('alias.save', { mac: editingAliasMac, alias }); closeModal(aliasModal) } function deleteAlias() { send('alias.delete', { mac: editingAliasMac }); closeModal(aliasModal) }
        function nodeTargets() { const members = selectedNodes(); if (!members.length) { alert('Select at least one Node'); return null } return { targetType: 'devices', members } } function licenseTargets() { const g = currentGroup(); return g ? { targetType: 'group', groupId: g.id } : nodeTargets() } function getSelectedLicense() { const d = licenseTargets(); if (d) send('license.get', d) } function openLicense() { const d = licenseTargets(); if (!d) return; const g = currentGroup(), picked = g ? groupDevices(g) : d.members.map(mac => devices.find(v => v.mac === mac)).filter(Boolean), first = picked[0], sameLid = picked.every(v => v.lid === first?.lid), remain = Number(first?.remain) || 0; licDeviceName.value = g ? `${g.name} (${picked.length} Devices)` : picked.map(deviceUid).join(', '); licLidField.style.display = g ? 'none' : ''; licLid.value = g ? '' : sameLid ? (first?.lid || '') : ''; licHours.value = Math.floor(remain / 60); licMinutes.value = remain % 60; licenseModal.classList.add('open') } function setLicense() { const d = licenseTargets(); if (!d) return; const g = currentGroup(), hours = +licHours.value, minutes = +licMinutes.value; d.durationMinutes = hours * 60 + minutes; d.expired = 0; if (!g) d.lid = +licLid.value; if ((!g && d.lid <= 0) || hours < 0 || minutes < 0 || minutes > 59 || d.durationMinutes <= 0) return alert(g ? 'Invalid duration' : 'Invalid LID or duration'); closeModal(licenseModal); send('license.set', d) }
        function openConfigDevice() { const mac = oneNode(); if (!mac) return alert('Select exactly one Node'); const x = devices.find(v => v.mac === mac) || groups.flatMap(groupDevices).find(v => v.mac === mac); if (!x) return alert('Selected Node was not found'); configDeviceUid.value = deviceUid(x) || ''; configCurrentLid.value = x.lid ?? ''; configNewLid.value = ''; configModal.classList.add('open'); configNewLid.focus() } function saveConfigDevice() { const mac = oneNode(); if (!mac) return alert('Select exactly one Node'); const newLid = configNewLid.value.trim(); if (!newLid) return alert('New LID is required'); closeModal(configModal); send('device.config', { targetType: 'devices', members: [mac], new_lid: newLid }) }
        function drawGroupMembers(selected = [], saved = []) { const picked = new Set(selected), all = new Map();[...saved, ...devices].forEach(d => all.set(d.mac, d)); groupMembers.innerHTML = all.size ? [...all.values()].map(d => `<label class="member"><input class="memberPick" type="checkbox" value="${esc(d.mac)}" ${picked.has(d.mac) || picked.has(deviceUid(d)) ? 'checked' : ''} onchange="updateMemberCount()"><span>${esc(displayDeviceId(d))} - ${esc(d.mac)}</span></label>`).join('') : '<div class="empty">Scan for Devices before creating a Group.</div>'; updateMemberCount() } function toggleAllGroupMembers(checked) { document.querySelectorAll('.memberPick').forEach(box => box.checked = checked); updateMemberCount() } function updateMemberCount() { const boxes = [...document.querySelectorAll('.memberPick')], selected = boxes.filter(box => box.checked).length; selectedMemberCount.textContent = selected; groupCheckAll.disabled = !boxes.length; groupCheckAll.checked = boxes.length > 0 && selected === boxes.length; groupCheckAll.indeterminate = selected > 0 && selected < boxes.length } function openGroup() { editingGroupId = 0; groupModalTitle.textContent = 'Add Group'; saveGroupButton.textContent = 'Create Group'; groupName.value = ''; drawGroupMembers(); groupModal.classList.add('open') } function editGroup() { const g = currentGroup(); if (!g) return alert('Select a Group from the Device filter'); editingGroupId = g.id; groupModalTitle.textContent = 'Edit Group'; saveGroupButton.textContent = 'Save Group'; groupName.value = g.name; drawGroupMembers(g.members || [], groupDevices(g)); groupModal.classList.add('open') } function saveGroup() { const name = groupName.value.trim(), members = [...document.querySelectorAll('.memberPick:checked')].map(x => x.value); if (!name) return alert('Enter a Group Name'); closeModal(groupModal); send('group.save', { id: editingGroupId, name, members }) } function confirmDeleteGroup() { const g = currentGroup(); if (!g) return alert('Select a Group from the Device filter'); deleteGroupText.textContent = `Delete Group "${g.name}" with ${(g.members || []).length} Devices?`; deleteGroupModal.classList.add('open') } function deleteGroup() { const g = currentGroup(); closeModal(deleteGroupModal); if (!g) return; groupFilter.value = 'all'; devicePage = 1; send('group.delete', { id: g.id }) }
        function confirmDeleteNode() { if (currentGroup()) return alert('Switch to All Devices before removing a temporary Device'); if (!oneNode()) return alert('Select exactly one Node'); deleteNodeModal.classList.add('open') } function deleteNode() { const mac = oneNode(); closeModal(deleteNodeModal); if (mac) send('node.delete', { mac }) } function closeModal(m) { m.classList.remove('open') } function log(s) { logEl.textContent = `[${new Date().toLocaleTimeString()}] ${s}\n` + logEl.textContent }
        function renderAccounts() { accountsEl.innerHTML = accounts.length ? accounts.map(a => `<tr><td><strong>${esc(a.username)}</strong>${a.username === currentUser ? ' (current)' : ''}</td><td><span class="status-pill ${a.twoFactorEnabled ? 'ONLINE' : 'UNKNOWN'}">${a.twoFactorEnabled ? 'Enabled' : 'Disabled'}</span></td><td><div class="account-actions"><button class="btn" onclick="openPassword('${esc(a.username)}')">Password</button>${a.username === currentUser && !a.twoFactorEnabled ? `<button class="btn primary" onclick="send('account.2fa.setup')">Enable 2FA</button>` : ''}${a.twoFactorEnabled ? `<button class="btn" onclick="disableTwoFactor('${esc(a.username)}')">Disable 2FA</button>` : ''}<button class="btn danger" onclick="deleteWebAccount('${esc(a.username)}')">Delete</button></div></td></tr>`).join('') : '<tr><td class="empty" colspan="3">No accounts.</td></tr>' }
        function openAccount() { editingAccount = ''; accountModalTitle.textContent = 'Add Account'; accountUsername.readOnly = false; accountUsername.value = ''; accountPassword.value = ''; accountModal.classList.add('open'); accountUsername.focus() } function openPassword(username) { editingAccount = username; accountModalTitle.textContent = `Change Password: ${username}`; accountUsername.value = username; accountUsername.readOnly = true; accountPassword.value = ''; accountModal.classList.add('open'); accountPassword.focus() } function saveAccount() { const username = accountUsername.value.trim(), password = accountPassword.value; if (!username || password.length < 6) return alert('Enter a valid username and password of at least 6 characters'); send(editingAccount ? 'account.password' : 'account.create', { username, password }); closeModal(accountModal) } function deleteWebAccount(username) { if (confirm(`Delete account "${username}"?`)) send('account.delete', { username }) } function disableTwoFactor(username) { if (confirm(`Disable 2FA for "${username}"?`)) send('account.2fa.disable', { username }) }
        function showTwoFactorSetup(m) { twoFactorSecret.textContent = m.secret || ''; const size = Number(m.qrSize) || 0, width = Math.ceil(size / 4), bits = []; for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) { const nibble = parseInt(m.qrData[y * width + Math.floor(x / 4)] || '0', 16); bits.push((nibble & (8 >> (x % 4))) ? '#000' : '#fff') } twoFactorQr.style.gridTemplateColumns = `repeat(${size},1fr)`; twoFactorQr.innerHTML = bits.map(color => `<i style="background:${color}"></i>`).join(''); twoFactorCode.value = ''; twoFactorModal.classList.add('open'); twoFactorCode.focus() } function confirmTwoFactor() { if (!/^\d{6}$/.test(twoFactorCode.value)) return alert('Enter the 6-digit authentication code'); send('account.2fa.confirm', { code: twoFactorCode.value, epoch: Math.floor(Date.now() / 1000) }); closeModal(twoFactorModal) }
        function hideMainViews() { controlsView.classList.add('hidden'); firmwareView.classList.add('hidden'); accountsView.classList.add('hidden') } function showControls(section) { hideMainViews(); controlsView.classList.remove('hidden'); document.querySelector('.page-title h1').textContent = 'CONTROLS SYSTEM'; const target = section === 'devices' ? deviceSection : section === 'groups' ? groupSection : overviewSection; target.scrollIntoView({ behavior: 'smooth', block: 'start' }) } function showFirmware() { hideMainViews(); firmwareView.classList.remove('hidden'); document.querySelector('.page-title h1').textContent = 'Firmware Update'; window.scrollTo({ top: 0, behavior: 'smooth' }) } function showAccounts() { hideMainViews(); accountsView.classList.remove('hidden'); document.querySelector('.page-title h1').textContent = 'Accounts'; send('account.list'); window.scrollTo({ top: 0, behavior: 'smooth' }) } function previewFirmwareUpdate() { if (!firmwareFile.files.length) alert('Choose a .bin file') }
        const ids = ['loginView', 'loginForm', 'loginUsername', 'loginPasswordField', 'loginPassword', 'loginOtpField', 'loginOtp', 'togglePassword', 'rememberLogin', 'loginError', 'dashboardView', 'logoutButton', 'hub', 'connection', 'connectionText', 'controlsView', 'firmwareView', 'accountsView', 'accountsEl', 'accountModal', 'accountModalTitle', 'accountUsername', 'accountPassword', 'twoFactorModal', 'twoFactorQr', 'twoFactorSecret', 'twoFactorCode', 'onlineCount', 'offlineCount', 'groupCount', 'scanLimit', 'job', 'scanProgress', 'listTitle', 'deviceCount', 'groupFilter', 'filterDevice', 'filterLid', 'checkAll', 'devicesEl', 'paginationSummary', 'paginationPages', 'previousPage', 'nextPage', 'logEl', 'currentVersion', 'firmwareFile', 'otaProgress', 'otaPercent', 'licenseModal', 'licDeviceName', 'licLidField', 'licLid', 'licHours', 'licMinutes', 'configModal', 'configDeviceUid', 'configCurrentLid', 'configNewLid', 'groupModal', 'groupModalTitle', 'groupName', 'groupCheckAll', 'groupMembers', 'selectedMemberCount', 'saveGroupButton', 'aliasModal', 'aliasDeviceId', 'aliasName', 'deleteAliasButton', 'deleteNodeModal', 'deleteGroupModal', 'deleteGroupText', 'overviewSection', 'deviceSection', 'groupSection']; ids.forEach(id => window[id] = $(id)); devicesEl.addEventListener('click', e => { const b = e.target.closest('.edit-alias'); if (b) openAlias(b.dataset.mac) }); devicesEl.addEventListener('change', e => { if (e.target.classList.contains('pick')) syncCheckAll() }); document.querySelectorAll('.nav-btn[data-section]').forEach(b => b.onclick = () => { document.querySelectorAll('.nav-btn[data-section]').forEach(x => x.classList.remove('active')); b.classList.add('active'); b.dataset.section === 'settings' ? showFirmware() : b.dataset.section === 'accounts' ? showAccounts() : showControls(b.dataset.section) }); initLogin();
    </script>
</body>

</html>

)HTML";

#endif // LOCAL_WEB_PAGE_H
