*** Settings ***
Library    Remote    http://127.0.0.1:8270

*** Test Cases ***
OD Read Write Smoke
    Write OD    0x2000    0    42
    ${value}=    Read OD    0x2000    0
    Should Be Equal    ${value}    42

OD Default Value Smoke
    ${default_value} =    Read OD    0x2001    0
    Should Be Equal    ${default_value}    Hello World

PDO Signal Smoke
    ${signals}=    List PDO Signals    receive    1
    Should Not Be Empty    ${signals}
