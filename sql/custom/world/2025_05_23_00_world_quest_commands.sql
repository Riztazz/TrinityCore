DELETE FROM `command`
WHERE `name` IN ('quest add', 'quest complete', 'quest remove', 'quest reward');

INSERT INTO `command` (`name`, `help`) VALUES
('quest add', 'Syntax: .quest add #quest_id [$playername]\r\n\r\nAdd to character quest log quest #quest_id for selected player or $playername if provided (or self if no player selected). Quest started from item can\'t be added by this command but correct .additem call provided in command output.'),
('quest complete', 'Syntax: .quest complete #questid [$playername]\r\nMark all quest objectives as completed for selected player or $playername if provided (or self if no player selected). After this, the player can go and get the quest reward.'),
('quest remove', 'Syntax: .quest remove #quest_id [$playername]\r\n\r\nSet quest #quest_id state to not completed and not active (and remove from active quest list) for selected player or $playername if provided (or self if no player selected).'),
('quest reward', 'Syntax: .quest reward #questId [$playername]\r\nGrants quest reward to selected player or $playername if provided (or self if no player selected) and removes quest from their log (quest must be in completed state).');