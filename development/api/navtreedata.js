/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "HF-SE050 Driver", "index.html", [
    [ "hf-se050-driver", "index.html", "index" ],
    [ "API reference (Phase 1)", "md_docs_2api__reference.html", null ],
    [ "CMake integration", "md_docs_2cmake__integration.html", null ],
    [ "Examples", "md_docs_2examples.html", [
      [ "Internal flow diagrams", "md_docs_2examples.html#autotoc_md14", [
        [ "se050_minimal_example", "md_docs_2examples.html#autotoc_md15", null ],
        [ "se050_smoke_example", "md_docs_2examples.html#autotoc_md16", null ],
        [ "se050_object_lifecycle_example", "md_docs_2examples.html#autotoc_md17", null ],
        [ "se050_cloud_onboarding_example", "md_docs_2examples.html#autotoc_md18", null ],
        [ "se050_cloud_registration_packet_example", "md_docs_2examples.html#autotoc_md19", null ]
      ] ],
      [ "Next reading", "md_docs_2examples.html#autotoc_md20", null ]
    ] ],
    [ "Hardware setup", "md_docs_2hardware__setup.html", null ],
    [ "HF-SE050 documentation", "md_docs_2index.html", [
      [ "Structure", "md_docs_2index.html#autotoc_md23", null ]
    ] ],
    [ "Installation", "md_docs_2installation.html", [
      [ "Doxygen (API HTML, local)", "md_docs_2installation.html#autotoc_md25", null ]
    ] ],
    [ "Platform integration (CRTP transport)", "md_docs_2platform__integration.html", [
      [ "Required methods (Derived)", "md_docs_2platform__integration.html#autotoc_md27", null ],
      [ "Optional", "md_docs_2platform__integration.html#autotoc_md28", null ],
      [ "Reference implementation", "md_docs_2platform__integration.html#autotoc_md29", null ],
      [ "Handler layer (hf-core)", "md_docs_2platform__integration.html#autotoc_md30", null ]
    ] ],
    [ "Quick start", "md_docs_2quickstart.html", null ],
    [ "Security flows with SE050 (easy-to-follow)", "md_docs_2security__iot__ota__comms.html", [
      [ "Security goals (keep these separate)", "md_docs_2security__iot__ota__comms.html#autotoc_md33", null ],
      [ "Which approach for board-to-board comms?", "md_docs_2security__iot__ota__comms.html#autotoc_md34", null ],
      [ "Ethernet implementation recipe (TLS-first)", "md_docs_2security__iot__ota__comms.html#autotoc_md35", [
        [ "Recommended Ethernet pattern", "md_docs_2security__iot__ota__comms.html#autotoc_md36", null ]
      ] ],
      [ "OTA trust chain (practical)", "md_docs_2security__iot__ota__comms.html#autotoc_md37", null ],
      [ "App-layer encrypted packet model (when not using TLS)", "md_docs_2security__iot__ota__comms.html#autotoc_md38", null ],
      [ "How current examples map to deployment steps", "md_docs_2security__iot__ota__comms.html#autotoc_md39", [
        [ "1) se050_minimal_example", "md_docs_2security__iot__ota__comms.html#autotoc_md40", null ],
        [ "2) se050_smoke_example", "md_docs_2security__iot__ota__comms.html#autotoc_md41", null ],
        [ "3) se050_object_lifecycle_example", "md_docs_2security__iot__ota__comms.html#autotoc_md42", null ],
        [ "4) se050_cloud_onboarding_example", "md_docs_2security__iot__ota__comms.html#autotoc_md43", null ],
        [ "5) se050_cloud_registration_packet_example", "md_docs_2security__iot__ota__comms.html#autotoc_md44", null ]
      ] ],
      [ "Security checklist for production", "md_docs_2security__iot__ota__comms.html#autotoc_md45", null ]
    ] ],
    [ "Troubleshooting", "md_docs_2troubleshooting.html", null ],
    [ "Medical-Device Cybersecurity — Full Chain, Explained", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html", [
      [ "The five trust anchors on the SE050", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md59", null ],
      [ "Re-provisioning (field-return / service depot)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md60", [
        [ "4.1 Token format (120 B)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md61", null ],
        [ "4.2 Why each field matters", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md62", null ],
        [ "4.3 Cryptographic invariants to preserve", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md63", null ],
        [ "4.4 Operational flow", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md64", null ]
      ] ],
      [ "TLS / cloud authentication", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md65", [
        [ "6.1 What mbedTLS does (transport)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md66", null ],
        [ "6.2 What the SE050 does (identity)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md67", null ],
        [ "6.3 AWS IoT policy hardening (least privilege)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md68", null ],
        [ "6.4 Device Defender", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md69", null ]
      ] ],
      [ "Logging and audit trail (non-PHI)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md70", null ],
      [ "Known limitations of this example (what ships today)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md71", null ],
      [ "What you still need to build (your side)", "md_examples_2esp32_2main_2se050__aws__iot__lifecycle_2SECURITY.html#autotoc_md72", null ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"index.html#autotoc_md5",
"namespacese050_1_1applet.html#af561b767a8b3409c7419141265b0b59a",
"se050__commands_8hpp.html#a0c3028ab8cf03900d7f9a54b0685a55b",
"stage__provisioning_8hpp.html#a431b5278b1d8177f3da343363f9c4d20"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';