Usbmodem daemon provides ubus API for each own interface.

All methods must to be called on this path:
`usbmodem.<interface_name>`

Where `<interface_name>` is name of modem interface in openwrt.

**List all available modems:**
```
$ ubus list usbmodem.*
usbmodem.LTE
```

# send_command

Sending custom AT command to modem.

**Arguments:**
| Name | Type | Description |
|---|---|---|
| command | string | AT command |
| timeout | int | Timeout for command response. Used default timeout, when omitted. |

**Response:**
| Name | Type | Description |
|---|---|---|
| success | string | True, if command success |
| response | string | Response of AT command. Can be empty, when timeout occured. |

**Example:**
```
$ ubus call usbmodem.LTE send_command '{"command": "AT+CGMI"}'
{
	"response": "+CGMI: \"Huawei\"\nOK",
	"success": true
}
```

# send_ussd

Sending USSD query to network.

**Arguments:**
| Name | Type | Description |
|---|---|---|
| query | string | USSD query, for example: `*777#` |
| answer | string | Answer for ussd, when previous USSD respond with code=1. |

You can pass **query** for new ussd query and **answer** for answer to previous query. But not at the same time.

**Response:**
| Name | Type | Description |
|---|---|---|
| code | int | USSD response code:<br>0 - Success<br>1 - Success, but wait answer<br>2 - Discard by network |
| response | string | Decoded USSD response. |
| error | string | Error description, when request failed. |

**Example:**
```
$ ubus call usbmodem.LTE send_ussd '{"query": "*777#"}'
{
	"code": 0,
	"response": "Your balance is 1.00 usd."
}
$ ubus call usbmodem.LTE send_ussd '{"query": "wefwefw"}'
{
	"error": "Not valid ussd command."
}
$ ubus call usbmodem.LTE send_ussd '{"query": "*123#"}'
{
	"code": 1,
	"response": "1) check balance\n2) show my number\n0) cancel"
}
$ ubus call usbmodem.LTE send_ussd '{"answer": "1"}'
{
	"code": 0,
	"response": "Your balance is 13.77 usd."
}
```

# cancel_ussd

Canceling current USSD session (when previous query respond with code=1 and waiting for reply)

**Example:**
```
$ ubus call usbmodem.LTE cancel_ussd
```
