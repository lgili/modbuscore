# Device Identification Support (FC2B) - Delta Spec

## ADDED Requirements

### Requirement: FC2B Request Parsing
The protocol module SHALL support parsing FC2B (Read Device Identification) requests.

#### Scenario: Parse valid FC2B request
- **GIVEN** FC2B request with MEI type 0x0E
- **AND** read device ID code (0x01-0x04)
- **AND** object ID (0x00-0xFF)
- **WHEN** request is parsed
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** MEI type SHALL be extracted
- **AND** read device ID code SHALL be extracted
- **AND** object ID SHALL be extracted

#### Scenario: Reject invalid MEI type
- **GIVEN** FC2B request with MEI type != 0x0E
- **WHEN** request is parsed
- **THEN** function SHALL return MBC_STATUS_INVALID_MEI
- **AND** exception response SHALL be prepared

### Requirement: Device Object Structure
The server SHALL maintain device identification objects with standard attributes.

#### Scenario: Device object storage
- **GIVEN** device object with ID, length, and ASCII value
- **WHEN** object is stored
- **THEN** object ID SHALL be 0x00-0xFF
- **AND** value length SHALL be <= 255 bytes
- **AND** value SHALL be ASCII encoded
- **AND** object SHALL be retrievable by ID

### Requirement: Standard Object Support
The server SHALL support standard Modbus device identification objects.

#### Scenario: Basic identification level objects
- **GIVEN** basic conformity level
- **WHEN** objects are registered
- **THEN** object 0x00 (Vendor Name) SHALL be supported
- **AND** object 0x01 (Product Code) SHALL be supported
- **AND** object 0x02 (Major/Minor Revision) SHALL be supported

#### Scenario: Regular identification level objects
- **GIVEN** regular conformity level
- **WHEN** objects are registered
- **THEN** basic objects (0x00-0x02) SHALL be supported
- **AND** object 0x03 (Vendor URL) SHALL be supported (optional)
- **AND** object 0x04 (Product Name) SHALL be supported (optional)
- **AND** object 0x05 (Model Name) SHALL be supported (optional)
- **AND** object 0x06 (User Application Name) SHALL be supported (optional)

### Requirement: Device Info Management API
The server SHALL provide API for configuring device identification.

#### Scenario: Add device object
- **GIVEN** object ID, ASCII value, and length
- **WHEN** `mbc_server_add_device_object()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** object SHALL be stored
- **AND** object count SHALL increment

#### Scenario: Set standard objects in bulk
- **GIVEN** vendor name, product code, and revision strings
- **WHEN** `mbc_server_set_device_info()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** objects 0x00, 0x01, 0x02 SHALL be created
- **AND** conformity level SHALL be set to BASIC

#### Scenario: Prevent duplicate object IDs
- **GIVEN** object ID 0x00 already exists
- **WHEN** attempting to add object 0x00 again
- **THEN** function SHALL return MBC_STATUS_ALREADY_EXISTS
- **OR** function SHALL update existing object (implementation choice)

### Requirement: Read Device ID Codes
The server SHALL support all standard read device ID codes.

#### Scenario: Read Basic (Code 0x01)
- **GIVEN** FC2B request with read device ID code 0x01
- **WHEN** request is processed
- **THEN** response SHALL include objects 0x00-0x02 only
- **AND** conformity level SHALL be indicated

#### Scenario: Read Regular (Code 0x02)
- **GIVEN** FC2B request with read device ID code 0x02
- **WHEN** request is processed
- **THEN** response SHALL include objects 0x00-0x06 (if available)
- **AND** conformity level SHALL be indicated

#### Scenario: Read Extended (Code 0x03)
- **GIVEN** FC2B request with read device ID code 0x03
- **WHEN** request is processed
- **THEN** response SHALL include objects 0x07-0xFF (if available)
- **AND** conformity level SHALL be indicated

#### Scenario: Read Specific Object (Code 0x04)
- **GIVEN** FC2B request with read device ID code 0x04
- **AND** specific object ID
- **WHEN** request is processed
- **THEN** response SHALL include only requested object
- **OR** exception if object not found

### Requirement: FC2B Response Building
The protocol SHALL build properly formatted FC2B responses.

#### Scenario: Single object response
- **GIVEN** FC2B request for single object
- **AND** object exists and fits in response
- **WHEN** response is built
- **THEN** response SHALL contain function code 0x2B
- **AND** MEI type 0x0E
- **AND** read device ID code
- **AND** conformity level
- **AND** more follows = 0x00
- **AND** next object ID = 0x00
- **AND** number of objects = 1
- **AND** object ID, length, and value

#### Scenario: Multiple object response
- **GIVEN** FC2B request for multiple objects
- **AND** all objects fit in single response
- **WHEN** response is built
- **THEN** response SHALL contain all requested objects
- **AND** more follows = 0x00
- **AND** number of objects = actual count

### Requirement: Multi-Packet Response Support
The protocol SHALL handle device info that exceeds single frame size.

#### Scenario: Response requires multiple packets
- **GIVEN** FC2B request results in > 253 bytes of data
- **WHEN** first response is built
- **THEN** more follows SHALL be 0xFF
- **AND** next object ID SHALL indicate continuation point
- **AND** number of objects SHALL be partial count

#### Scenario: Subsequent packet request
- **GIVEN** previous response indicated more follows
- **AND** FC2B request with starting object ID
- **WHEN** continuation response is built
- **THEN** response SHALL start from requested object ID
- **AND** more follows SHALL indicate if additional packets needed

### Requirement: Conformity Level Configuration
The server SHALL support conformity level configuration.

#### Scenario: Set basic conformity level
- **GIVEN** objects 0x00-0x02 registered
- **WHEN** conformity level is queried
- **THEN** conformity level SHALL be BASIC (0x01)

#### Scenario: Set regular conformity level
- **GIVEN** objects 0x00-0x06 registered
- **WHEN** conformity level is queried
- **THEN** conformity level SHALL be REGULAR (0x02)

#### Scenario: Set extended conformity level
- **GIVEN** objects 0x00-0xFF registered (including extended)
- **WHEN** conformity level is queried
- **THEN** conformity level SHALL be EXTENDED (0x03)

### Requirement: Exception Handling
The protocol SHALL properly handle FC2B exceptions.

#### Scenario: Requested object not found
- **GIVEN** FC2B request for non-existent object
- **WHEN** request is processed
- **THEN** exception response SHALL be sent
- **AND** exception code SHALL be 0x02 (Illegal Data Address)

#### Scenario: Invalid read device ID code
- **GIVEN** FC2B request with invalid code (> 0x04)
- **WHEN** request is processed
- **THEN** exception response SHALL be sent
- **AND** exception code SHALL be 0x03 (Illegal Data Value)

### Requirement: Memory Efficiency
Device info storage SHALL be memory efficient for MCU constraints.

#### Scenario: Predictable memory usage
- **GIVEN** device info structure
- **WHEN** calculating memory requirements
- **THEN** memory SHALL be <= 20 bytes overhead per object
- **AND** total memory SHALL be configurable at compile time
- **AND** maximum objects SHALL be definable (e.g., 10, 20, 50)

### Requirement: ASCII Validation
Object values SHALL be validated as valid ASCII.

#### Scenario: Accept valid ASCII
- **GIVEN** object value contains only ASCII characters (0x20-0x7E)
- **WHEN** object is added
- **THEN** function SHALL return MBC_STATUS_OK

#### Scenario: Reject non-ASCII
- **GIVEN** object value contains non-ASCII bytes
- **WHEN** object is added
- **THEN** function SHALL return MBC_STATUS_INVALID_DATA
- **OR** function SHALL sanitize data (implementation choice)

## MODIFIED Requirements

None - This is a new feature addition.

## REMOVED Requirements

None.

## RENAMED Requirements

None.
