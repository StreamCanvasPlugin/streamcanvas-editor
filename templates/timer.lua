local countdown_duration = 300 

local end_time = os.time() + countdown_duration

function _on_trigger_in()
    end_time = os.time() + countdown_duration
end

function _get_data()
    local current_time = os.time()
    local remaining_seconds = end_time - current_time

    if remaining_seconds < 0 then
        remaining_seconds = 0
    end

    -- Calculate total minutes and remaining seconds
    local minutes = math.floor(remaining_seconds / 60)
    local seconds = remaining_seconds % 60

    -- Extract digits and convert them directly to strings
    local n0 = tostring(math.floor(minutes / 10)) -- Tens of minutes
    local n1 = tostring(minutes % 10)             -- Units of minutes
    local n2 = tostring(math.floor(seconds / 10)) -- Tens of seconds
    local n3 = tostring(seconds % 10)             -- Units of seconds

    -- Return the list of objects containing the string digits
    return {
        { n0 = n0, n1 = n1, n2 = n2, n3 = n3 }
    }
end
