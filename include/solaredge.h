/*******************************************************************************
 * @file    solaredge.h
 * @brief   Certificat SSL SolarEdge API
 * @details Certificat racine DigiCert pour communication HTTPS sécurisée avec
 *          l'API SolarEdge (monitoring.solaredge.com). Permet récupération
 *          production solaire temps réel.
 * 
 * Usage   : HTTPClient secure vers API SolarEdge
 * Référencé par : main.cpp (si API SolarEdge configurée)
 * Référence     : WiFiClientSecure
 * 
 * @author  Ludovic Sorriaux
 * @date    2024
 *******************************************************************************/

    // Solaredge certificate
    const char* solarEdgeCertificate = \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIG0jCCBbqgAwIBAgIQBTSk/jsp6vtgsSMhimJ3FTANBgkqhkiG9w0BAQsFADBP\n" \
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMSkwJwYDVQQDEyBE\n" \
    "aWdpQ2VydCBUTFMgUlNBIFNIQTI1NiAyMDIwIENBMTAeFw0yMzA1MDIwMDAwMDBa\n" \
    "Fw0yNDA2MDEyMzU5NTlaMHYxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9y\n" \
    "bmlhMREwDwYDVQQHEwhNaWxwaXRhczElMCMGA1UEChMcU29sYXJFZGdlIFRlY2hu\n" \
    "b2xvZ2llcywgSW5jLjEYMBYGA1UEAwwPKi5Tb2xhckVkZ2UuY29tMIIBIjANBgkq\n" \
    "hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyWyO3CIxPyDxXd6naLC99PVZhBiWq4/7\n" \
    "SaA9PCWtUeBw8k+uy5ON/ufXrz/3sixtBGgNrRbF+OFri8jhbT2Qaf7h1j9M79pF\n" \
    "7RGXK1duiLJ3xkfin3TmqZnBJZP2gMJCBSRIf9GIkb0bMavXrn6pHXDWNShBSTW8\n" \
    "I6D+xej1hEY5lr9xW+lKN0l8hNCavCYbe3nvUgDdqcPNxpmnmgN9xSwY4ry399RK\n" \
    "ACIbGm9qI1O9QtiDLIYbx8qw/6Oef2fJxnS4z1C3bdXacU3O8znIwLxGSjJsxCwz\n" \
    "ZeWgeoC+pZPa9jVsFfzVDNHll55m9lOIqBmX9Ibs4H6ZQj6wb6ICZQIDAQABo4ID\n" \
    "gTCCA30wHwYDVR0jBBgwFoAUt2ui6qiqhIx56rTaD5iyxZV2ufQwHQYDVR0OBBYE\n" \
    "FEpFehSBXBRYleTKOZq7YTDcYNmIMCkGA1UdEQQiMCCCDyouU29sYXJFZGdlLmNv\n" \
    "bYINc29sYXJlZGdlLmNvbTAOBgNVHQ8BAf8EBAMCBaAwHQYDVR0lBBYwFAYIKwYB\n" \
    "BQUHAwEGCCsGAQUFBwMCMIGPBgNVHR8EgYcwgYQwQKA+oDyGOmh0dHA6Ly9jcmwz\n" \
    "LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFRMU1JTQVNIQTI1NjIwMjBDQTEtNC5jcmww\n" \
    "QKA+oDyGOmh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFRMU1JTQVNI\n" \
    "QTI1NjIwMjBDQTEtNC5jcmwwPgYDVR0gBDcwNTAzBgZngQwBAgIwKTAnBggrBgEF\n" \
    "BQcCARYbaHR0cDovL3d3dy5kaWdpY2VydC5jb20vQ1BTMH8GCCsGAQUFBwEBBHMw\n" \
    "cTAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tMEkGCCsGAQUF\n" \
    "BzAChj1odHRwOi8vY2FjZXJ0cy5kaWdpY2VydC5jb20vRGlnaUNlcnRUTFNSU0FT\n" \
    "SEEyNTYyMDIwQ0ExLTEuY3J0MAkGA1UdEwQCMAAwggGBBgorBgEEAdZ5AgQCBIIB\n" \
    "cQSCAW0BawB3AO7N0GTV2xrOxVy3nbTNE6Iyh0Z8vOzew1FIWUZxH7WbAAABh9tV\n" \
    "NrgAAAQDAEgwRgIhALXTBmmiQAMclMwRzUMXLapPjNteanzT2PRbg5+24iFpAiEA\n" \
    "2OVnxJ9Y/yDMvgnXVRnHOeoEctABdT+8ELt88TWtlegAdwBz2Z6JG0yWeKAgfUed\n" \
    "5rLGHNBRXnEZKoxrgBB6wXdytQAAAYfbVTakAAAEAwBIMEYCIQDgbU3TQjlRNuyJ\n" \
    "Eu9UC4AMRzbwj6QNahCL4ivGEQW4SQIhAMkWb3GVF7VSZuT+0Vpis92IoWNLHhpw\n" \
    "ZjQuWjamydP3AHcASLDja9qmRzQP5WoC+p0w6xxSActW3SyB2bu/qznYhHMAAAGH\n" \
    "21U2cQAABAMASDBGAiEA9n5dljzpgiWHo9cyD8X4AGzve5wRu/rh9jxX22P0UAUC\n" \
    "IQDm14Jc/1qzNbuIoDvtvc3XhxdjWNwnAyIwr3LeLk1BzDANBgkqhkiG9w0BAQsF\n" \
    "AAOCAQEAmVBS00yPVyz/KS9B/fGLVikT4AeAKjMRLUYRxPiwXwo93HANUCjjtx3r\n" \
    "/wdNerxkE4VmsCUI8xjWmI+NmXYN4WESdgM1Ig2edVVEseRLGl/pGdpMyrF/mhHd\n" \
    "Lss8eO30arJ4DzlYhiKmhhXGC69woTOW3XimClLppHUphC3Kkz/uxGYPO7XkrvPM\n" \
    "tb9neKPh0fYjXEQsCtBmMka9zEymXYJEoKjJBIt5FT4pyYly2v/1/eRN6+zFixNa\n" \
    "pZqEJNZvGIjK1X0l3h+TpAXolFUr5azMelOhJlDaqohvSbDjSiabwOU78C3Vult6\n" \
    "ujlccKwldLY6gzrqUIKmgYaWHK/vQA==\n" \
    "-----END CERTIFICATE-----\n";
