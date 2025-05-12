-- Insert into map_parts and get the new id
INSERT INTO map_parts (map, part) VALUES (0, 1);
SET @last_map_part_id = LAST_INSERT_ID();

-- Now use that id in the next insert
INSERT INTO map_part_points (mapPartID, x, y) VALUES (@last_map_part_id, -9042.086, 56.44667);
INSERT INTO map_part_points (mapPartID, x, y) VALUES (@last_map_part_id, -9071.319, -119.50309);
INSERT INTO map_part_points (mapPartID, x, y) VALUES (@last_map_part_id, -9220.083, -297.67245);
INSERT INTO map_part_points (mapPartID, x, y) VALUES (@last_map_part_id, -9150.673, -496.25613);
INSERT INTO map_part_points (mapPartID, x, y) VALUES (@last_map_part_id, -8764.4375, -546.8778);
INSERT INTO map_part_points (mapPartID, x, y) VALUES (@last_map_part_id, -8471.799, -2.8025894);
INSERT INTO map_part_points (mapPartID, x, y) VALUES (@last_map_part_id, -8894.832, 200.85204);