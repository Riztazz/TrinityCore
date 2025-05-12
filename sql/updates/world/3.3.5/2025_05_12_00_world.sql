DROP TABLE IF EXISTS `map_partitions`;

CREATE TABLE `map_partitions`  (
  `id` INT NOT NULL AUTO_INCREMENT,
  `mapId` INT NOT NULL,
  `partitionId` INT NOT NULL,
  `polygon` TEXT NOT NULL,
  PRIMARY KEY (`id`)
);

INSERT INTO map_partitions (mapId, partitionId, polygon) VALUES
(
  0,
  1,
  '[{"x":-9042.086,"y":56.44667},{"x":-9071.319,"y":-119.50309},{"x":-9220.083,"y":-297.67245},{"x":-9150.673,"y":-496.25613},{"x":-8764.4375,"y":-546.8778},{"x":-8471.799,"y":-2.8025894},{"x":-8894.832,"y":200.85204}]'
);