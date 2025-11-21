import numpy as np


def filter_sids(all_sids: list[str], filter_string: str) -> list[str]:
    # The sids are taken as strings instead of ints to make it easier
    # for the C++ side to call this and process the output.
    all_sid_ints = [int(s) for s in all_sids]
    max_sid = max(all_sid_ints)

    sid_strings = filter_string.split(',')
    valid_sids = []
    for this_str in sid_strings:
        if ':' in this_str:
            this_slice = slice(
                *(int(s) if s else None for s in this_str.split(':'))
            )
            # If there was no stop specified, go to the end of the sid_list
            if this_slice.stop is None:
                this_slice = slice(this_slice.start, max_sid + 1,
                                   this_slice.step)

            valid_sids += np.r_[this_slice].tolist()
        else:
            valid_sids.append(int(this_str))

    # Sort and return
    return [str(sid) for sid in sorted(all_sid_ints) if sid in valid_sids]
