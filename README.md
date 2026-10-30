# Simplified blockchain simulation

## Requirements

C++11 or higher is required in order for the simulation to work.

### Transaction serialization

Transactions are serialized in this fashion:

```
<sender>|<receiver>|<amount>|<sequence>
```
